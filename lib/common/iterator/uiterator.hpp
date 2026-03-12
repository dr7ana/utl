#pragma once

#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace utl {
    template <typename LHS, typename RHS>
    concept same_unqualified = std::same_as<std::remove_cv_t<LHS>, std::remove_cv_t<RHS>>;

    template <typename T>
    class uiterator {
      public:
        using iterator_concept = std::contiguous_iterator_tag;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = std::remove_cv_t<T>;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;

        constexpr uiterator() noexcept = default;
        constexpr explicit uiterator(pointer ptr) noexcept : _ptr{ptr} {}

        template <typename U>
            requires(std::convertible_to<U*, pointer> && same_unqualified<T, U>)
        constexpr uiterator(const uiterator<U>& other) noexcept : _ptr{other.base()} {}

        [[nodiscard]] constexpr pointer base() const noexcept { return _ptr; }

        [[nodiscard]] constexpr reference operator*() const noexcept {
            assert(_ptr != nullptr && "iterator dereference on null pointer");
            return *_ptr;
        }

        [[nodiscard]] constexpr pointer operator->() const noexcept {
            assert(_ptr != nullptr && "iterator member access on null pointer");
            return _ptr;
        }

        [[nodiscard]] constexpr reference operator[](difference_type offset) const noexcept {
            return *(*this + offset);
        }

        constexpr uiterator& operator++() noexcept {
            ++_ptr;
            return *this;
        }

        constexpr uiterator operator++(int) noexcept {
            uiterator out{*this};
            ++(*this);
            return out;
        }

        constexpr uiterator& operator--() noexcept {
            --_ptr;
            return *this;
        }

        constexpr uiterator operator--(int) noexcept {
            uiterator out{*this};
            --(*this);
            return out;
        }

        constexpr uiterator& operator+=(difference_type offset) noexcept {
            _ptr += offset;
            return *this;
        }

        constexpr uiterator& operator-=(difference_type offset) noexcept {
            _ptr -= offset;
            return *this;
        }

        [[nodiscard]] constexpr uiterator operator+(difference_type offset) const noexcept {
            return uiterator{_ptr + offset};
        }

        [[nodiscard]] constexpr uiterator operator-(difference_type offset) const noexcept {
            return uiterator{_ptr - offset};
        }

        template <typename U>
            requires(same_unqualified<T, U>)
        [[nodiscard]] constexpr difference_type operator-(const uiterator<U>& other) const noexcept {
            return _ptr - other.base();
        }

        template <typename U>
            requires(same_unqualified<T, U>)
        [[nodiscard]] constexpr bool operator==(const uiterator<U>& other) const noexcept {
            return _ptr == other.base();
        }

        template <typename U>
            requires(same_unqualified<T, U>)
        [[nodiscard]] constexpr auto operator<=>(const uiterator<U>& other) const noexcept {
            return _ptr <=> other.base();
        }

        [[nodiscard]] friend constexpr uiterator operator+(difference_type offset, const uiterator& it) noexcept {
            return it + offset;
        }

      private:
        template <typename>
        friend class uiterator;

        pointer _ptr{nullptr};
    };

    static_assert(std::contiguous_iterator<uiterator<int>>);
    static_assert(std::contiguous_iterator<uiterator<const int>>);
}  // namespace utl
