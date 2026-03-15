#pragma once

#include <concepts>
#include <cstddef>
#include <exception>
#include <iostream>
#include <ostream>
#include <source_location>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace utl::test {
    //

    class failure : public std::runtime_error {
      public:
        using std::runtime_error::runtime_error;
    };

    struct test_case {
        std::string_view name;
        void (*fn)();
    };

    template <typename T>
    concept streamable = requires(std::ostream& out, const T& value) { out << value; };

    [[noreturn]] inline void fail(
            std::string_view msg, const std::source_location& source_location = std::source_location::current()) {
        std::ostringstream out;
        out << source_location.file_name() << ":" << source_location.line() << ": " << msg;
        throw failure{out.str()};
    }

    template <typename T>
    [[nodiscard]] inline std::string describe(const T& value) {
        using value_t = std::remove_cvref_t<T>;

        if constexpr (std::same_as<value_t, bool>) {
            return value ? "true" : "false";
        } else if constexpr (std::convertible_to<value_t, std::string_view>) {
            return std::string{std::string_view{value}};
        } else if constexpr (streamable<value_t>) {
            std::ostringstream out;
            out << value;
            return out.str();
        } else {
            return "<unprintable>";
        }
    }

    template <typename T>
        requires std::convertible_to<T, bool>
    struct REQUIRE_TRUE {
        explicit REQUIRE_TRUE(
                T&& value, const std::source_location& source_location = std::source_location::current()) {
            if (!static_cast<bool>(value)) {
                fail("expected condition to be true", source_location);
            }
        }
    };

    template <typename T>
        requires std::convertible_to<T, bool>
    struct REQUIRE_FALSE {
        explicit REQUIRE_FALSE(
                T&& value, const std::source_location& source_location = std::source_location::current()) {
            if (static_cast<bool>(value)) {
                fail("expected condition to be false", source_location);
            }
        }
    };

    template <typename lhs_t, typename rhs_t>
        requires requires(const std::remove_reference_t<lhs_t>& lhs, const std::remove_reference_t<rhs_t>& rhs) {
            { lhs == rhs } -> std::convertible_to<bool>;
        }
    struct REQUIRE_EQ {
        REQUIRE_EQ(
                lhs_t&& lhs,
                rhs_t&& rhs,
                const std::source_location& source_location = std::source_location::current()) {
            if (!(lhs == rhs)) {
                std::ostringstream out;
                out << "expected equality, lhs=" << describe(lhs) << ", rhs=" << describe(rhs);
                fail(out.str(), source_location);
            }
        }
    };

    template <typename lhs_t, typename rhs_t>
        requires requires(const std::remove_reference_t<lhs_t>& lhs, const std::remove_reference_t<rhs_t>& rhs) {
            { lhs != rhs } -> std::convertible_to<bool>;
        }
    struct REQUIRE_NE {
        REQUIRE_NE(
                lhs_t&& lhs,
                rhs_t&& rhs,
                const std::source_location& source_location = std::source_location::current()) {
            if (!(lhs != rhs)) {
                std::ostringstream out;
                out << "expected inequality, lhs=" << describe(lhs) << ", rhs=" << describe(rhs);
                fail(out.str(), source_location);
            }
        }
    };

    template <typename fn_t>
        requires std::invocable<fn_t>
    struct REQUIRE_THROWS {
        explicit REQUIRE_THROWS(
                fn_t&& fn, const std::source_location& source_location = std::source_location::current()) {
            bool threw{};
            try {
                std::forward<fn_t>(fn)();
            } catch (...) {
                threw = true;
            }

            if (!threw) {
                fail("expected callable to throw", source_location);
            }
        }
    };

    template <typename T>
    REQUIRE_TRUE(T&& value) -> REQUIRE_TRUE<T>;

    template <typename T>
    REQUIRE_FALSE(T&& value) -> REQUIRE_FALSE<T>;

    template <typename lhs_t, typename rhs_t>
    REQUIRE_EQ(lhs_t&& lhs, rhs_t&& rhs) -> REQUIRE_EQ<lhs_t, rhs_t>;

    template <typename lhs_t, typename rhs_t>
    REQUIRE_NE(lhs_t&& lhs, rhs_t&& rhs) -> REQUIRE_NE<lhs_t, rhs_t>;

    template <typename fn_t>
    REQUIRE_THROWS(fn_t&& fn) -> REQUIRE_THROWS<fn_t>;

    inline int run_suite(std::string_view suite_name, std::span<const test_case> tests) {
        size_t fail_count{};

        for (const test_case& test : tests) {
            try {
                test.fn();
                std::cout << "[PASS] " << suite_name << "::" << test.name << "\n";
            } catch (const std::exception& e) {
                ++fail_count;
                std::cout << "[FAIL] " << suite_name << "::" << test.name << ": " << e.what() << "\n";
            } catch (...) {
                ++fail_count;
                std::cout << "[FAIL] " << suite_name << "::" << test.name << ": unknown exception\n";
            }
        }

        if (fail_count != 0) {
            std::cout << suite_name << ": " << fail_count << " failure(s)\n";
            return 1;
        }

        std::cout << suite_name << ": all tests passed\n";
        return 0;
    }
}  // namespace utl::test
