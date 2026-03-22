#pragma once

#include <allocazam.hpp>

#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace utl {
    //

    inline constexpr size_t dynamic_extent{static_cast<size_t>(-1)};

    template <size_t Size>
    concept valid_ring_size = Size > 0 && std::has_single_bit(Size);

    template <size_t Size>
    concept valid_ring_extent = Size == dynamic_extent || valid_ring_size<Size>;

    template <typename T>
    concept valid_spsc_value_type = std::is_object_v<T> && std::destructible<T>;

    template <typename T, bool HugePages = false>
    using spsc_allocator = allocazam::allocazam_std_allocator<
            T,
            allocazam::memory_mode::fixed,
            HugePages ? allocazam::huge_pages::enabled : allocazam::huge_pages::disabled>;

    template <typename Alloc, typename Value>
    concept valid_spsc_allocator = requires { typename Alloc::state_type; } &&
                                   std::same_as<typename std::allocator_traits<Alloc>::value_type, Value> &&
                                   std::same_as<typename std::allocator_traits<Alloc>::pointer, Value*> &&
                                   std::same_as<typename std::allocator_traits<Alloc>::const_pointer, const Value*> &&
                                   std::is_constructible_v<typename Alloc::state_type, size_t> &&
                                   std::is_constructible_v<Alloc, typename Alloc::state_type&>;

    template <valid_spsc_value_type T, size_t RingSize, valid_spsc_allocator<T> A = spsc_allocator<T>>
        requires valid_ring_extent<RingSize>
    class spsc_queue {
        static constexpr size_t static_ring_size{RingSize};
        static constexpr bool dynamic_capacity{RingSize == dynamic_extent};
        static constexpr bool fixed_capacity{not dynamic_capacity};
        static constexpr bool nothrow_value_destructible{std::is_nothrow_destructible_v<T>};

      public:
        using value_type = T;
        using allocator_type = A;
        using state_type = typename allocator_type::state_type;
        using allocator_traits = std::allocator_traits<allocator_type>;
        using size_type = size_t;
        using pointer = value_type*;
        using const_pointer = const value_type*;

        template <typename F>
        static constexpr bool valid_producer =
                std::invocable<F&&, pointer> && std::is_nothrow_invocable_v<F&&, pointer> &&
                (std::same_as<std::invoke_result_t<F&&, pointer>, void> ||
                 std::same_as<std::invoke_result_t<F&&, pointer>, bool>);

        template <typename... Args>
        static constexpr bool valid_emplace =
                std::constructible_from<value_type, Args...> && std::is_nothrow_constructible_v<value_type, Args...>;

        template <typename F>
        static constexpr bool valid_consumer = std::invocable<F, value_type&>;

        static constexpr bool pop_assignable = std::assignable_from<value_type&, value_type&&>;
        static constexpr bool optional_pop_enabled = std::move_constructible<value_type>;

        constexpr spsc_queue()
            requires fixed_capacity
                : spsc_queue{static_ring_size, init_tag{}} {}

        constexpr explicit spsc_queue(size_type ring_size)
            requires dynamic_capacity
                : spsc_queue{_validate_ring_size(ring_size), init_tag{}} {}

        constexpr ~spsc_queue() noexcept(nothrow_value_destructible) {
            clear();
            allocator_traits::deallocate(_alloc, _base, _ring_size);
        }

        spsc_queue(const spsc_queue&) = delete;
        spsc_queue& operator=(const spsc_queue&) = delete;
        spsc_queue(spsc_queue&&) = delete;
        spsc_queue& operator=(spsc_queue&&) = delete;

        [[nodiscard]] static consteval size_type static_capacity() noexcept { return static_ring_size; }
        [[nodiscard]] constexpr size_type capacity() const noexcept { return _ring_size; }

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
            return (tail - head) >= _ring_size;
        }

        /*
            Producer contract:
            - slot points to uninitialized storage for exactly one value_type
            - fn must begin lifetime for exactly one value_type at slot (typically std::construct_at(slot, ...))
                example: q.produce([&](value_type* slot) noexcept { std::construct_at(slot, args...); });
            - fn must not read *slot before construction
            - fn must be noexcept; if fn throws after construction, a live unpublished object is leaked in the ring
            - returning true means the slot was published (tail advanced) and queue owns destruction on consume/clear
         */
        template <typename F>
            requires valid_producer<F>
        constexpr bool produce(F&& fn) {
            size_type tail = _tail.load(std::memory_order_relaxed);
            size_type head = _head.load(std::memory_order_acquire);

            if ((tail - head) >= _ring_size) {
                return false;
            }

            pointer slot = _base + _slot_index(tail);

            if constexpr (std::same_as<std::invoke_result_t<F&&, pointer>, bool>) {
                if (!std::forward<F>(fn)(slot)) {
                    return false;
                }
            } else {
                std::forward<F>(fn)(slot);
            }

            _tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        template <typename... Args>
            requires valid_emplace<Args...>
        constexpr bool emplace(Args&&... args) {
            return produce([&](pointer slot) noexcept {
                allocator_traits::construct(_alloc, slot, std::forward<Args>(args)...);
            });
        }

        constexpr bool push(const value_type& value) { return emplace(value); }

        constexpr bool push(value_type&& value) { return emplace(std::move(value)); }

        /*
            Consumer exception guarantee (basic):
            - consume(fn): if fn throws, head is not advanced; front element remains live but may be modified by fn
            - consume_n/consume_all(fn): head advances only for callbacks that returned normally; on throw, the
              throwing element remains at head and may already be modified by fn
            - pop(...) wrappers forward to consume(...) and inherit this guarantee
         */
        template <typename F>
            requires valid_consumer<F>
        constexpr bool consume(F&& fn) {
            size_type head = _head.load(std::memory_order_relaxed);
            size_type tail = _tail.load(std::memory_order_acquire);

            if (head == tail) {
                return false;
            }

            pointer slot = _base + _slot_index(head);
            std::forward<F>(fn)(*slot);
            allocator_traits::destroy(_alloc, slot);
            _head.store(head + 1, std::memory_order_release);
            return true;
        }

        template <typename F>
            requires valid_consumer<F>
        constexpr size_type consume_n(size_type max, F&& fn) {
            if (max == 0) {
                return 0;
            }

            size_type head = _head.load(std::memory_order_relaxed);
            size_type tail = _tail.load(std::memory_order_acquire);
            size_type available = tail - head;

            size_type target = available < max ? available : max;
            if (target == 0) {
                return 0;
            }

            auto&& consumer = fn;
            return _consume_batch(head, target, consumer);
        }

        template <typename F>
            requires valid_consumer<F>
        constexpr size_type consume_all(F&& fn) {
            size_type head = _head.load(std::memory_order_relaxed);
            size_type tail = _tail.load(std::memory_order_acquire);
            size_type available = tail - head;

            if (available == 0) {
                return 0;
            }

            auto&& consumer = fn;
            return _consume_batch(head, available, consumer);
        }

        constexpr bool pop(value_type& out)
            requires pop_assignable
        {
            return consume([&](value_type& value) { out = std::move(value); });
        }

        [[nodiscard]] constexpr std::optional<value_type> pop()
            requires optional_pop_enabled
        {
            std::optional<value_type> out{};
            if (!consume([&](value_type& value) { out.emplace(std::move(value)); })) {
                return std::nullopt;
            }
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
        struct init_tag {};

        constexpr spsc_queue(size_type ring_size, init_tag)
                : _ring_size{ring_size},
                  _state{_state_pool_size_for_ring(ring_size)},
                  _alloc{_state},
                  _base{allocator_traits::allocate(_alloc, _ring_size)} {}

        template <typename F>
            requires valid_consumer<F>
        constexpr size_type _consume_batch(size_type head, size_type count, F& fn) {
            size_type consumed{};
            try {
                for (; consumed < count; ++consumed) {
                    pointer slot = _base + _slot_index(head + consumed);
                    fn(*slot);
                    allocator_traits::destroy(_alloc, slot);
                }
            } catch (...) {
                if (consumed != 0) {
                    _head.store(head + consumed, std::memory_order_release);
                }
                throw;
            }

            _head.store(head + consumed, std::memory_order_release);
            return consumed;
        }

        [[nodiscard]] static constexpr size_type _state_pool_size_for_ring(size_type ring_size) noexcept {
            return std::bit_ceil(ring_size + size_type{1});
        }

        [[nodiscard]] static constexpr bool _valid_dynamic_ring_size(size_type ring_size) noexcept {
            return ring_size > 0 && std::has_single_bit(ring_size);
        }

        [[nodiscard]] static constexpr size_type _validate_ring_size(size_type ring_size) {
            if (_valid_dynamic_ring_size(ring_size)) {
                return ring_size;
            }

            throw std::invalid_argument{"utl::spsc_queue ring_size must be a non-zero power of two"};
        }

        [[nodiscard]] constexpr size_type _slot_index(size_type cursor) const noexcept {
            if constexpr (fixed_capacity) {
                return cursor & (static_ring_size - size_type{1});
            }

            return cursor & (_ring_size - size_type{1});
        }

        size_type _ring_size{0};
        state_type _state;
        [[no_unique_address]] allocator_type _alloc{};
        pointer _base{nullptr};

        alignas(64) std::atomic<size_type> _head{};
        alignas(64) std::atomic<size_type> _tail{};
    };

    template <typename T, size_t RingSize>
    using huge_spsc_queue = spsc_queue<T, RingSize, spsc_allocator<T, true>>;
}  // namespace utl
