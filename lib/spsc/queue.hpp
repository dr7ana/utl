#pragma once

#include <allocazam.hpp>

#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace utl {
    //

    template <size_t Size>
    concept valid_ring_size = Size > 0 && std::has_single_bit(Size);

    template <typename T>
    concept valid_spsc_value_type =
            std::copy_constructible<T> && std::move_constructible<T> && std::assignable_from<T&, T&&>;

    template <typename Alloc, typename Value>
    concept valid_spsc_allocator = requires { typename Alloc::state_type; } &&
                                   std::same_as<typename std::allocator_traits<Alloc>::value_type, Value> &&
                                   std::is_constructible_v<typename Alloc::state_type, size_t> &&
                                   std::is_constructible_v<Alloc, typename Alloc::state_type&>;

    template <
            valid_spsc_value_type T,
            size_t RingSize,
            typename A = allocazam::allocazam_std_allocator<T, allocazam::memory_mode::fixed>>
        requires valid_ring_size<RingSize> && valid_spsc_allocator<A, T>
    class spsc_queue {
        static constexpr size_t ring_size{RingSize};
        static constexpr size_t _state_pool_size_for_ring{std::bit_ceil(ring_size + size_t{1})};
        static constexpr bool nothrow_value_destructible{std::is_nothrow_destructible_v<T>};

      public:
        using value_type = T;
        using allocator_type = A;
        using state_type = typename allocator_type::state_type;
        using allocator_traits = std::allocator_traits<allocator_type>;
        using size_type = size_t;
        using pointer = typename allocator_traits::pointer;
        using const_pointer = typename allocator_traits::const_pointer;

        constexpr spsc_queue()
                : _state{_state_pool_size_for_ring},
                  _alloc{_state},
                  _base{allocator_traits::allocate(_alloc, ring_size)} {}

        constexpr ~spsc_queue() noexcept(nothrow_value_destructible) {
            clear();
            allocator_traits::deallocate(_alloc, _base, ring_size);
        }

        spsc_queue(const spsc_queue&) = delete;
        spsc_queue& operator=(const spsc_queue&) = delete;
        spsc_queue(spsc_queue&&) = delete;
        spsc_queue& operator=(spsc_queue&&) = delete;

        [[nodiscard]] static consteval size_type static_capacity() noexcept { return ring_size; }
        [[nodiscard]] constexpr size_type capacity() const noexcept { return ring_size; }

        [[nodiscard]] constexpr size_type size() const noexcept {
            size_type head = _head.load(std::memory_order_acquire);
            size_type tail = _tail.load(std::memory_order_acquire);
            return tail - head;
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            size_type head = _head.load(std::memory_order_acquire);
            size_type tail = _tail.load(std::memory_order_acquire);
            return head == tail;
        }

        [[nodiscard]] constexpr bool full() const noexcept {
            size_type head = _head.load(std::memory_order_acquire);
            size_type tail = _tail.load(std::memory_order_acquire);
            return (tail - head) >= ring_size;
        }

        template <typename... Args>
            requires(std::constructible_from<value_type, Args...>)
        constexpr bool emplace(Args&&... args) {
            size_type tail = _tail.load(std::memory_order_relaxed);
            size_type head = _head.load(std::memory_order_acquire);

            if ((tail - head) >= ring_size) {
                return false;
            }

            pointer slot = _base + _slot_index(tail);
            allocator_traits::construct(_alloc, slot, std::forward<Args>(args)...);
            _tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        constexpr bool push(const value_type& value) { return emplace(value); }

        constexpr bool push(value_type&& value) { return emplace(std::move(value)); }

        constexpr bool pop(value_type& out) {
            size_type head = _head.load(std::memory_order_relaxed);
            size_type tail = _tail.load(std::memory_order_acquire);

            if (head == tail) {
                return false;
            }

            pointer slot = _base + _slot_index(head);
            out = std::move(*slot);
            allocator_traits::destroy(_alloc, slot);
            _head.store(head + 1, std::memory_order_release);
            return true;
        }

        [[nodiscard]] constexpr std::optional<value_type> pop() {
            size_type head = _head.load(std::memory_order_relaxed);
            size_type tail = _tail.load(std::memory_order_acquire);

            if (head == tail) {
                return std::nullopt;
            }

            pointer slot = _base + _slot_index(head);
            std::optional<value_type> out{std::in_place, std::move(*slot)};
            allocator_traits::destroy(_alloc, slot);
            _head.store(head + 1, std::memory_order_release);
            return out;
        }

        constexpr void clear() noexcept(nothrow_value_destructible) {
            size_type head = _head.load(std::memory_order_relaxed);
            size_type tail = _tail.load(std::memory_order_relaxed);

            while (head != tail) {
                pointer slot = _base + _slot_index(head);
                allocator_traits::destroy(_alloc, slot);
                ++head;
            }

            _head.store(0, std::memory_order_relaxed);
            _tail.store(0, std::memory_order_relaxed);
        }

      private:
        [[nodiscard]] static constexpr size_type _slot_index(size_type cursor) noexcept {
            return cursor & (ring_size - size_type{1});
        }

        state_type _state;
        [[no_unique_address]] allocator_type _alloc{};
        pointer _base{nullptr};

        alignas(64) std::atomic<size_type> _head{};
        alignas(64) std::atomic<size_type> _tail{};
    };
}  // namespace utl
