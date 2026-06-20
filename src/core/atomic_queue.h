#ifndef ATOMIC_QUEUE_H
#define ATOMIC_QUEUE_H

#include "core/memory_manager.h"
#include <array>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <limits>

namespace bsm {

template <typename T> struct AtomicSlot {
  auto take() -> T {
    T object_to_return{std::move(object_)};
    object_.~T();
    is_initialized_.store(false);
    return object_to_return;
  }
  template <typename U>
    requires std::convertible_to<U, T>
  auto emplace(U&& object) -> bool {
    new (&object_) T{std::forward<U>(object)};
    return true;
  }

  std::atomic_bool is_initialized_{false};
  T object_;
};

template <typename T> class AtomicQueue {
public:
  AtomicQueue() {
    memory_queue.init();
    for (uint64_t i{0}; i < RING_BUFFER_SIZE; i++) {
      auto allocation_result{memory_queue.allocate()};
      new (allocation_result->memory_ptr) AtomicSlot<T>{};
    }
  }

  auto try_pop() -> std::optional<T> {
    uint64_t last_busy_index = head.load();
    if (last_busy_index == tail) {
      return std::nullopt;
    }
    uint64_t normalized_ring_slot{normilize_ring_slot(last_busy_index)};
    if (head.compare_exchange_strong(last_busy_index, last_busy_index + 1)) {
      auto* raw_atomic_slot{memory_queue[normalized_ring_slot]};
      auto& atomic_slot{
          *std::launder(static_cast<AtomicSlot<T>*>(raw_atomic_slot)),
      };
      while (true) {
        atomic_slot.is_initialized_.wait(false);
        if (atomic_slot.is_initialized_.load()) {
          T data = atomic_slot.take();
          atomic_slot.is_initialized_.store(false);
          atomic_slot.is_initialized_.notify_one();
          return data;
        }
      }
    }
    return std::nullopt;
  }

  template <typename U>
    requires std::convertible_to<U, T>
  auto try_push(U&& object) -> bool {
    while (true) {
      uint64_t last_free_index = tail.load();
      uint64_t last_busy_index = head.load();
      uint64_t normalized_ring_slot{normilize_ring_slot(last_free_index)};
      uint64_t normalized_busy{normilize_ring_slot(last_busy_index)};
      if ((normalized_ring_slot + 1) == normalized_busy) {
        return false;
      }
      if (tail.compare_exchange_strong(last_free_index, last_free_index + 1U)) {
        auto* raw_atomic_slot{memory_queue[normalized_ring_slot]};
        auto& atomic_slot{
            *std::launder(static_cast<AtomicSlot<T>*>(raw_atomic_slot)),
        };
        do {
          atomic_slot.is_initialized_.wait(true);
        } while (atomic_slot.is_initialized_.load());
            atomic_slot.emplace(std::forward<U>(object));
            atomic_slot.is_initialized_.store(true);
            atomic_slot.is_initialized_.notify_one();
            return true;
          }
        }
      }
    }
  }

  auto pop() -> T* {
    uint64_t last_busy_index = head.load();
    T* data{nullptr};
    while (data == nullptr) {
      queue[last_busy_index].wait(data);
      data = queue[last_busy_index].exchange(nullptr);
    }
    head.compare_exchange_strong(last_busy_index, last_busy_index + 1);
    queue[last_busy_index].notify_one();
    return data;
  }

  void wait_for_data() {
    uint16_t last_busy_index = head.load();
    T* data{nullptr};
    while (data == nullptr) {
      queue[last_busy_index].wait(data);
      if (queue[last_busy_index] != nullptr) {
        break;
      }
    }
  }

  void mmanager_wait_for_data() {
    uint16_t last_busy_index = head.load();
    uint64_t normalized_ring_slot{normilize_ring_slot(last_busy_index)};
    auto* raw_atomic_slot{memory_queue[normalized_ring_slot]};
    auto& atomic_slot{
        *std::launder(static_cast<AtomicSlot<T>*>(raw_atomic_slot)),
    };
    while (true) {
      atomic_slot.is_initialized_.wait(false);
      if (atomic_slot.is_initialized_.load()) {
        break;
      }
    }
  }

private:
  auto normilize_ring_slot(uint64_t ring_slot) -> uint64_t {
    constexpr uint64_t RING_MASK = RING_BUFFER_SIZE - 1;
    return ring_slot & RING_MASK;
  }
  static constexpr uint64_t RING_BUFFER_SIZE = 4096;

  std::atomic_uint64_t head{0};
  std::atomic_uint64_t tail{0};
  std::array<std::atomic<T*>, std::numeric_limits<uint16_t>::max() + 1> queue{};
  MemoryManager memory_queue;
};

} // namespace bsm

#endif
