#pragma once

#include "iterator/uiterator.hpp"

#include <allocazam.hpp>

namespace utl {
    //

    template <class T, class A = allocazam::allocazam_std_allocator<T, allocazam::memory_mode::dynamic>>
    class uvector {
        //
      public:
        using value_type = T;
        using allocator_type = A;
        using size_type = std::size_t;

        using reference = value_type&;
        using const_reference = const value_type&;

        using iterator = uiterator<T>;
        using const_iterator = uiterator<const T>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        using pointer = typename iterator::pointer;
        using const_pointer = typename const_iterator::pointer;
        using difference_type = typename iterator::difference_type;

        //

        constexpr uvector() noexcept(noexcept(allocator_type())) : uvector{allocator_type()} {}

        explicit constexpr uvector(const allocator_type&) noexcept(noexcept(allocator_type()));

        explicit constexpr uvector(size_type, const allocator_type& = allocator_type());

        constexpr uvector(size_type, const T&, const allocator_type& = allocator_type());

        template <typename InputIterator>
        constexpr uvector(InputIterator, InputIterator, const allocator_type& = allocator_type());

        constexpr uvector(const uvector&);

        constexpr uvector(uvector&&) noexcept;

        constexpr uvector(const uvector&, const allocator_type&);

        constexpr uvector(uvector&&, const allocator_type&);

        constexpr ~uvector();

        constexpr uvector& operator=(const uvector&);

        constexpr uvector& operator=(uvector&&) noexcept;

        //
        constexpr decltype(auto) operator[](this auto& self, size_type);

        //
        constexpr decltype(auto) at(this auto& self, size_type);

        //
        constexpr decltype(auto) front(this auto& self);

        //
        constexpr decltype(auto) back(this auto& self);

        //
        constexpr decltype(auto) data(this auto& self) noexcept;

        //
        constexpr decltype(auto) begin(this auto& self) noexcept;

        //
        constexpr decltype(auto) end(this auto& self) noexcept;

        //
        constexpr decltype(auto) rbegin(this auto& self) noexcept;

        //
        constexpr decltype(auto) rend(this auto& self) noexcept;

        //
        constexpr const_iterator cbegin() const noexcept;
        constexpr const_reverse_iterator crbegin() const noexcept;

        //
        constexpr const_iterator cend() const noexcept;
        constexpr const_reverse_iterator crend() const noexcept;

        //

        //
        template <typename... Arg>
        constexpr reference& emplace_back(Arg&&...);

        //
        constexpr void push_back(const T&);
        constexpr void push_back(T&&);

        //
        template <typename... Arg>
        constexpr iterator emplace(const_iterator, Arg&&...);

        //
        constexpr iterator insert(const_iterator, const T&);
        constexpr iterator insert(const_iterator, T&&);
        constexpr iterator insert(const_iterator, size_type, const T&);

        template <typename InputIterator>
        constexpr iterator insert(const_iterator, InputIterator, InputIterator);

        //
        constexpr iterator erase(iterator);
        constexpr iterator erase(const_iterator);

        constexpr iterator erase(iterator, iterator);
        constexpr iterator erase(const_iterator, const_iterator);

        //
        constexpr void reserve(size_type);

        // TODO: pending allocator ::expand update
        constexpr void resize(size_type);
        constexpr void resize(size_type, const T&);

        //
        constexpr void pop_back();

        //
        constexpr void swap(uvector&&) noexcept;

        //
        constexpr bool empty() const noexcept;
        constexpr size_type size() const noexcept;
        constexpr size_type max_size() const noexcept;
        constexpr size_type capacity() const noexcept;

        //
        constexpr bool operator==(const uvector&);
        constexpr auto operator<=>(const uvector&);
    };
}  // namespace utl
