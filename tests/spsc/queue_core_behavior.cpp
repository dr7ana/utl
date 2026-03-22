#include "utl/spsc/queue.hpp"

#include "util/test.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {
    struct tracked {
        inline static int live_count{};

        int value{};

        tracked() = delete;
        explicit tracked(int value) noexcept : value{value} { ++live_count; }
        tracked(const tracked&) = delete;
        tracked& operator=(const tracked&) = delete;

        tracked(tracked&& other) noexcept : value{other.value} { ++live_count; }

        tracked& operator=(tracked&& other) noexcept {
            value = other.value;
            return *this;
        }

        ~tracked() { --live_count; }
    };

    struct consume_error : std::runtime_error {
        consume_error() : std::runtime_error{"consume callback failure"} {}
    };

#if defined(__linux__)
    constexpr size_t huge_page_kb = 2048;

    struct mapping_info {
        size_t kernel_page_kb{};
        bool hugetlb{};
    };

    [[nodiscard]] bool parse_mapping_header(const std::string& line, uintptr_t& begin, uintptr_t& end) noexcept {
        size_t dash = line.find('-');
        size_t space = line.find(' ');
        if (dash == std::string::npos || space == std::string::npos || dash >= space) {
            return false;
        }

        const char* data = line.data();
        auto [begin_end, begin_ec] = std::from_chars(data, data + dash, begin, 16);
        auto [end_end, end_ec] = std::from_chars(data + dash + 1, data + space, end, 16);
        return begin_ec == std::errc{} && end_ec == std::errc{} && begin_end == (data + dash) &&
               end_end == (data + space);
    }

    [[nodiscard]] size_t parse_kb_value(const std::string& line) noexcept {
        size_t colon = line.find(':');
        if (colon == std::string::npos) {
            return 0;
        }

        const char* first = line.data() + colon + 1;
        const char* last = line.data() + line.size();
        while (first != last && *first == ' ') {
            ++first;
        }

        size_t value = 0;
        auto [end, ec] = std::from_chars(first, last, value);
        if (ec != std::errc{} || end == first) {
            return 0;
        }

        return value;
    }

    [[nodiscard]] bool vmflags_contains(const std::string& line, std::string_view needle) noexcept {
        size_t colon = line.find(':');
        if (colon == std::string::npos) {
            return false;
        }

        std::string_view rest{line.data() + colon + 1, line.size() - colon - 1};
        while (!rest.empty()) {
            size_t first = rest.find_first_not_of(' ');
            if (first == std::string_view::npos) {
                return false;
            }

            rest.remove_prefix(first);

            size_t end = rest.find(' ');
            std::string_view token = rest.substr(0, end);
            if (token == needle) {
                return true;
            }

            if (end == std::string_view::npos) {
                return false;
            }

            rest.remove_prefix(end + 1);
        }

        return false;
    }

    [[nodiscard]] std::optional<mapping_info> mapping_for_address(const void* p) {
        std::ifstream smaps{"/proc/self/smaps"};
        if (!smaps.is_open()) {
            return std::nullopt;
        }

        uintptr_t target = reinterpret_cast<uintptr_t>(p);
        std::string line;
        bool in_target = false;
        mapping_info info{};

        while (std::getline(smaps, line)) {
            uintptr_t begin{};
            uintptr_t end{};
            if (parse_mapping_header(line, begin, end)) {
                if (in_target) {
                    return info;
                }

                in_target = begin <= target && target < end;
                if (in_target) {
                    info = {};
                }
                continue;
            }

            if (!in_target) {
                continue;
            }

            std::string_view view{line};
            if (view.starts_with("KernelPageSize:")) {
                info.kernel_page_kb = parse_kb_value(line);
            } else if (view.starts_with("VmFlags:")) {
                info.hugetlb = vmflags_contains(line, "ht");
            }
        }

        if (in_target) {
            return info;
        }

        return std::nullopt;
    }
#endif

    void test_produce_consume_round_trip() {
        utl::spsc_queue<int, 8> queue{};
        int seen{};

        utl::test::REQUIRE_TRUE{queue.empty()};
        utl::test::REQUIRE_EQ{queue.capacity(), size_t{8}};

        utl::test::REQUIRE_TRUE{queue.produce([&](int* slot) noexcept { std::construct_at(slot, 17); })};
        utl::test::REQUIRE_EQ{queue.size(), size_t{1}};
        utl::test::REQUIRE_FALSE{queue.empty()};

        utl::test::REQUIRE_TRUE{queue.consume([&](int& value) { seen = value; })};
        utl::test::REQUIRE_EQ{seen, 17};
        utl::test::REQUIRE_TRUE{queue.empty()};
    }

    void test_dynamic_extent_round_trip() {
        utl::spsc_queue<int, utl::dynamic_extent> queue{8};
        int seen{};

        utl::test::REQUIRE_EQ{decltype(queue)::static_capacity(), utl::dynamic_extent};
        utl::test::REQUIRE_EQ{queue.capacity(), size_t{8}};
        utl::test::REQUIRE_TRUE{queue.emplace(23)};
        utl::test::REQUIRE_TRUE{queue.consume([&](int& value) { seen = value; })};
        utl::test::REQUIRE_EQ{seen, 23};
        utl::test::REQUIRE_TRUE{queue.empty()};
    }

    void test_dynamic_extent_rejects_invalid_capacity() {
        bool threw{};

        try {
            [[maybe_unused]] utl::spsc_queue<int, utl::dynamic_extent> queue{3};
        } catch (const std::invalid_argument&) {
            threw = true;
        }

        utl::test::REQUIRE_TRUE{threw};
    }

    void test_emplace_push_and_batch_drains() {
        utl::spsc_queue<int, 8> queue{};
        std::array<int, 3> seen{};
        size_t index{};

        utl::test::REQUIRE_TRUE{queue.emplace(1)};
        utl::test::REQUIRE_TRUE{queue.push(2)};
        utl::test::REQUIRE_TRUE{queue.emplace(3)};

        utl::test::REQUIRE_EQ{queue.consume_n(2, [&](int& value) { seen[index++] = value; }), size_t{2}};
        utl::test::REQUIRE_EQ{seen[0], 1};
        utl::test::REQUIRE_EQ{seen[1], 2};
        utl::test::REQUIRE_EQ{queue.size(), size_t{1}};

        utl::test::REQUIRE_EQ{queue.consume_all([&](int& value) { seen[index++] = value; }), size_t{1}};
        utl::test::REQUIRE_EQ{seen[2], 3};
        utl::test::REQUIRE_TRUE{queue.empty()};
    }

    void test_full_and_wraparound() {
        utl::spsc_queue<int, 4> queue{};
        std::array<int, 6> seen{};
        size_t index{};
        bool invoked{};

        utl::test::REQUIRE_TRUE{queue.emplace(0)};
        utl::test::REQUIRE_TRUE{queue.emplace(1)};
        utl::test::REQUIRE_TRUE{queue.emplace(2)};
        utl::test::REQUIRE_TRUE{queue.emplace(3)};

        utl::test::REQUIRE_TRUE{queue.full()};
        utl::test::REQUIRE_FALSE{queue.produce([&](int* slot) noexcept {
            invoked = true;
            std::construct_at(slot, 99);
        })};
        utl::test::REQUIRE_FALSE{invoked};

        utl::test::REQUIRE_EQ{queue.consume_n(2, [&](int& value) { seen[index++] = value; }), size_t{2}};

        utl::test::REQUIRE_TRUE{queue.emplace(4)};
        utl::test::REQUIRE_TRUE{queue.emplace(5)};

        utl::test::REQUIRE_EQ{queue.consume_all([&](int& value) { seen[index++] = value; }), size_t{4}};

        utl::test::REQUIRE_EQ{seen[0], 0};
        utl::test::REQUIRE_EQ{seen[1], 1};
        utl::test::REQUIRE_EQ{seen[2], 2};
        utl::test::REQUIRE_EQ{seen[3], 3};
        utl::test::REQUIRE_EQ{seen[4], 4};
        utl::test::REQUIRE_EQ{seen[5], 5};
        utl::test::REQUIRE_TRUE{queue.empty()};
    }

    void test_clear_destroys_live_values() {
        tracked::live_count = 0;

        {
            utl::spsc_queue<tracked, 4> queue{};

            utl::test::REQUIRE_TRUE{queue.emplace(1)};
            utl::test::REQUIRE_TRUE{queue.emplace(2)};
            utl::test::REQUIRE_TRUE{queue.emplace(3)};
            utl::test::REQUIRE_EQ{tracked::live_count, 3};

            queue.clear();
            utl::test::REQUIRE_EQ{tracked::live_count, 0};
            utl::test::REQUIRE_TRUE{queue.empty()};
        }

        utl::test::REQUIRE_EQ{tracked::live_count, 0};
    }

    void test_consume_exception_keeps_modified_front() {
        utl::spsc_queue<int, 8> queue{};
        bool saw_throw{};
        int seen{};

        utl::test::REQUIRE_TRUE{queue.emplace(7)};

        try {
            (void)queue.consume([&](int& value) {
                value = 70;
                throw consume_error{};
            });
        } catch (const consume_error&) {
            saw_throw = true;
        }

        utl::test::REQUIRE_TRUE{saw_throw};
        utl::test::REQUIRE_EQ{queue.size(), size_t{1}};
        utl::test::REQUIRE_TRUE{queue.consume([&](int& value) { seen = value; })};
        utl::test::REQUIRE_EQ{seen, 70};
    }

    void test_consume_n_advances_completed_only() {
        utl::spsc_queue<int, 8> queue{};
        std::array<int, 2> remaining{};
        size_t index{};
        bool saw_throw{};

        utl::test::REQUIRE_TRUE{queue.emplace(1)};
        utl::test::REQUIRE_TRUE{queue.emplace(2)};
        utl::test::REQUIRE_TRUE{queue.emplace(3)};
        utl::test::REQUIRE_TRUE{queue.emplace(4)};

        try {
            (void)queue.consume_n(4, [&](int& value) {
                if (value == 3) {
                    value = 30;
                    throw consume_error{};
                }
            });
        } catch (const consume_error&) {
            saw_throw = true;
        }

        utl::test::REQUIRE_TRUE{saw_throw};
        utl::test::REQUIRE_EQ{queue.size(), size_t{2}};
        utl::test::REQUIRE_EQ{queue.consume_all([&](int& value) { remaining[index++] = value; }), size_t{2}};
        utl::test::REQUIRE_EQ{remaining[0], 30};
        utl::test::REQUIRE_EQ{remaining[1], 4};
    }

    void test_pop_wrappers_round_trip() {
        utl::spsc_queue<int, 4> queue{};
        int out{};

        utl::test::REQUIRE_TRUE{queue.emplace(11)};
        utl::test::REQUIRE_TRUE{queue.pop(out)};
        utl::test::REQUIRE_EQ{out, 11};

        utl::test::REQUIRE_TRUE{queue.emplace(22)};
        std::optional<int> opt = queue.pop();
        utl::test::REQUIRE_TRUE{opt.has_value()};
        utl::test::REQUIRE_EQ{*opt, 22};
        utl::test::REQUIRE_TRUE{queue.empty()};
    }

#if defined(__linux__)
    void test_huge_fixed_queue_mapping_and_round_trip() {
        utl::huge_spsc_queue<int, 8> queue{};
        int* first_slot{};
        int seen{};

        utl::test::REQUIRE_TRUE{queue.produce([&](int* slot) noexcept {
            first_slot = slot;
            std::construct_at(slot, 41);
        })};

        utl::test::REQUIRE_NE{first_slot, nullptr};

        auto* header = reinterpret_cast<std::byte*>(first_slot) - sizeof(size_t);
        std::optional<mapping_info> info = mapping_for_address(header);
        utl::test::REQUIRE_TRUE{info.has_value()};
        utl::test::REQUIRE_EQ{info->kernel_page_kb, huge_page_kb};
        utl::test::REQUIRE_TRUE{info->hugetlb};

        utl::test::REQUIRE_TRUE{queue.consume([&](int& value) { seen = value; })};
        utl::test::REQUIRE_EQ{seen, 41};
        utl::test::REQUIRE_TRUE{queue.empty()};
    }

    void test_huge_dynamic_extent_round_trip() {
        utl::spsc_queue<int, utl::dynamic_extent, utl::spsc_allocator<int, true>> queue{8};
        int seen{};

        utl::test::REQUIRE_EQ{queue.capacity(), size_t{8}};
        utl::test::REQUIRE_TRUE{queue.emplace(91)};
        utl::test::REQUIRE_TRUE{queue.consume([&](int& value) { seen = value; })};
        utl::test::REQUIRE_EQ{seen, 91};
        utl::test::REQUIRE_TRUE{queue.empty()};
    }
#endif

    constexpr std::array tests{
#if defined(__linux__)
            utl::test::test_case{
                    "huge_fixed_queue_mapping_and_round_trip", test_huge_fixed_queue_mapping_and_round_trip},
            utl::test::test_case{"huge_dynamic_extent_round_trip", test_huge_dynamic_extent_round_trip},
#endif
            utl::test::test_case{"produce_consume_round_trip", test_produce_consume_round_trip},
            utl::test::test_case{"dynamic_extent_round_trip", test_dynamic_extent_round_trip},
            utl::test::test_case{
                    "dynamic_extent_rejects_invalid_capacity", test_dynamic_extent_rejects_invalid_capacity},
            utl::test::test_case{"emplace_push_and_batch_drains", test_emplace_push_and_batch_drains},
            utl::test::test_case{"full_and_wraparound", test_full_and_wraparound},
            utl::test::test_case{"clear_destroys_live_values", test_clear_destroys_live_values},
            utl::test::test_case{"consume_exception_keeps_modified_front", test_consume_exception_keeps_modified_front},
            utl::test::test_case{"consume_n_advances_completed_only", test_consume_n_advances_completed_only},
            utl::test::test_case{"pop_wrappers_round_trip", test_pop_wrappers_round_trip},
    };
}  // namespace

int main() {
    return utl::test::run_suite("spsc_queue_core_behavior", tests);
}
