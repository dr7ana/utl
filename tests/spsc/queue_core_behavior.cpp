#include "utl/spsc/queue.hpp"

#include "util/test.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>

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

    constexpr std::array tests{
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
