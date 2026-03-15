#pragma once

#include "utl/iterator/uiterator.hpp"

#include <allocazam.hpp>

#include <cassert>
#include <memory>
#include <utility>

namespace utl {
    //

    template <class T, class A = allocazam::allocazam_std_allocator<T, allocazam::memory_mode::dynamic>>
    class uvector {
        //
        static constexpr size_t _min_initial_capacity{2};

      public:
        using value_type = T;
        using allocator_type = A;
        using allocator_traits = std::allocator_traits<allocator_type>;
        using size_type = size_t;

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

        // template <typename InputIterator>
        // constexpr uvector(InputIterator, InputIterator, const allocator_type& = allocator_type());

        constexpr uvector(const uvector&);

        constexpr uvector(uvector&&) noexcept;

        constexpr uvector(const uvector&, const allocator_type&);

        constexpr uvector(uvector&&, const allocator_type&);

        constexpr ~uvector();

        constexpr uvector& operator=(const uvector&);

        constexpr uvector& operator=(uvector&&) noexcept;

        // TODO:
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

        //
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

      private:
        [[nodiscard]] static constexpr size_type _normalized_capacity(size_type count) noexcept {
            return std::ranges::max(_min_initial_capacity, count);
        }

        constexpr void _release_storage() noexcept;
        constexpr void _steal_storage_from(uvector&&) noexcept;

        //
        [[no_unique_address]] allocator_type _alloc{};

        //
        pointer _base{nullptr};

        //
        size_type _count{};
        size_type _capacity{};
    };

    template <class T, class A>
    constexpr uvector<T, A>::uvector(const allocator_type& alloc) noexcept(noexcept(allocator_type()))
            : _alloc{alloc} {}

    template <class T, class A>
    constexpr uvector<T, A>::uvector(size_type count, const allocator_type& alloc) : uvector{alloc} {
        if (count == 0) {
            return;
        }

        size_type requested_capacity = _normalized_capacity(count);
        pointer base = _alloc.allocate(requested_capacity);

        size_type built{};
        try {
            for (; built < count; ++built) {
                _alloc.construct(base + built);
            }
        } catch (...) {
            while (built > 0) {
                --built;
                _alloc.destroy(base + built);
            }
            _alloc.deallocate(base, requested_capacity);
            throw;
        }

        _base = base;
        _count = count;
        _capacity = requested_capacity;
    }

    template <class T, class A>
    constexpr uvector<T, A>::uvector(size_type count, const T& value, const allocator_type& alloc) : uvector{alloc} {
        if (count == 0) {
            return;
        }

        size_type requested_capacity = _normalized_capacity(count);
        pointer base = _alloc.allocate(requested_capacity);

        size_type built{};
        try {
            for (; built < count; ++built) {
                _alloc.construct(base + built, value);
            }
        } catch (...) {
            while (built > 0) {
                --built;
                _alloc.destroy(base + built);
            }
            _alloc.deallocate(base, requested_capacity);
            throw;
        }

        _base = base;
        _count = count;
        _capacity = requested_capacity;
    }

    template <class T, class A>
    constexpr uvector<T, A>::~uvector() {
        _release_storage();
    }

    template <class T, class A>
    constexpr uvector<T, A>::uvector(const uvector& other)
            : uvector{allocator_traits::select_on_container_copy_construction(other._alloc)} {
        if (other._count == 0) {
            return;
        }

        size_type requested_capacity = _normalized_capacity(other._count);
        pointer base = _alloc.allocate(requested_capacity);

        size_type built{};
        try {
            for (; built < other._count; ++built) {
                _alloc.construct(base + built, other._base[built]);
            }
        } catch (...) {
            while (built > 0) {
                --built;
                _alloc.destroy(base + built);
            }
            _alloc.deallocate(base, requested_capacity);
            throw;
        }

        _base = base;
        _count = other._count;
        _capacity = requested_capacity;
    }

    template <class T, class A>
    constexpr uvector<T, A>::uvector(uvector&& other) noexcept : _alloc{std::move(other._alloc)} {
        _steal_storage_from(std::move(other));
    }

    template <class T, class A>
    constexpr uvector<T, A>::uvector(const uvector& other, const allocator_type& alloc) : uvector{alloc} {
        if (other._count == 0) {
            return;
        }

        size_type requested_capacity = _normalized_capacity(other._count);
        pointer base = _alloc.allocate(requested_capacity);

        size_type built{};
        try {
            for (; built < other._count; ++built) {
                _alloc.construct(base + built, other._base[built]);
            }
        } catch (...) {
            while (built > 0) {
                --built;
                _alloc.destroy(base + built);
            }
            _alloc.deallocate(base, requested_capacity);
            throw;
        }

        _base = base;
        _count = other._count;
        _capacity = requested_capacity;
    }

    template <class T, class A>
    constexpr uvector<T, A>::uvector(uvector&& other, const allocator_type& alloc) : uvector{alloc} {
        if (other._count == 0) {
            return;
        }

        if (_alloc == other._alloc) {
            _steal_storage_from(std::move(other));
            return;
        }

        size_type requested_capacity = _normalized_capacity(other._count);
        pointer base = _alloc.allocate(requested_capacity);

        size_type built{};
        try {
            for (; built < other._count; ++built) {
                _alloc.construct(base + built, std::move(other._base[built]));
            }
        } catch (...) {
            while (built > 0) {
                --built;
                _alloc.destroy(base + built);
            }
            _alloc.deallocate(base, requested_capacity);
            throw;
        }

        _base = base;
        _count = other._count;
        _capacity = requested_capacity;
    }

    template <class T, class A>
    constexpr uvector<T, A>& uvector<T, A>::operator=(const uvector& other) {
        if (this == std::addressof(other)) {
            return *this;
        }

        if constexpr (allocator_traits::propagate_on_container_copy_assignment::value) {
            uvector tmp{other, other._alloc};
            *this = std::move(tmp);
        } else {
            uvector tmp{other, _alloc};
            *this = std::move(tmp);
        }

        return *this;
    }

    template <class T, class A>
    constexpr uvector<T, A>& uvector<T, A>::operator=(uvector&& other) noexcept {
        if (this == std::addressof(other)) {
            return *this;
        }

        _release_storage();

        if constexpr (allocator_traits::propagate_on_container_move_assignment::value) {
            _alloc = std::move(other._alloc);
        } else {
            if constexpr (!allocator_traits::is_always_equal::value) {
                assert(_alloc == other._alloc && "move assignment requires equal allocators when not propagating");
            }
        }

        _steal_storage_from(std::move(other));
        return *this;
    }

    template <class T, class A>
    constexpr void uvector<T, A>::_release_storage() noexcept {
        while (_count > 0) {
            --_count;
            _alloc.destroy(_base + _count);
        }

        if (_base != nullptr) {
            _alloc.deallocate(_base, _capacity);
            _base = nullptr;
        }

        _capacity = 0;
    }

    template <class T, class A>
    constexpr void uvector<T, A>::_steal_storage_from(uvector&& other) noexcept {
        _base = std::exchange(other._base, nullptr);
        _count = std::exchange(other._count, size_type{});
        _capacity = std::exchange(other._capacity, size_type{});
    }
}  // namespace utl
