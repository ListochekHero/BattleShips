#ifndef ATOMIC_QUEUE_H
#define ATOMIC_QUEUE_H

#include "core/memory_manager.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>

namespace bsm {

template <typename T> struct AtomicSlot {
  std::atomic_bool is_initialized{false};
  T object;
};

template <typename T> class AtomicQueue {
public:
  // AtomicQueue() { queue.fill(nullptr); }

  auto try_pop() -> T* {
    uint16_t last_busy_index = head.load();
    if (last_busy_index == tail) {
      return nullptr;
    }
    if (head.compare_exchange_strong(last_busy_index, last_busy_index + 1)) {
      T* data{nullptr};
      while (data == nullptr) {
        queue[last_busy_index].wait(data);
        data = queue[last_busy_index].exchange(nullptr);
      }
      queue[last_busy_index].notify_one();
      return data;
    }
    return nullptr;
  }
  auto mmanager_try_pop() -> T {
    uint64_t last_busy_index = head.load();
    if (last_busy_index == tail) {
      return nullptr;
    }
    if (head.compare_exchange_strong(last_busy_index, last_busy_index + 1)) {
      uint64_t normalized_ring_slot{normilize_ring_slot(last_busy_index)};
      auto* raw_atomic_slot{memory_queue[normalized_ring_slot]};
      auto& atomic_slot{
          *std::launder(static_cast<AtomicSlot<T>*>(raw_atomic_slot)),
      };
      while (true) {
        atomic_slot.is_initialized.wait(false);
        if (atomic_slot.is_initialized.load()) {
          T data = atomic_slot.take();
          atomic_slot.is_initialized.store(false);
          atomic_slot.is_initialized.notify_one();
          return data;
        }
      }
    }
    return nullptr;
  }
  auto push(T* ptr) -> bool {
    while (true) {
      uint16_t last_free_index = tail.load();
      if ((last_free_index + 1) == head) {
        return false;
      }
      if (tail.compare_exchange_strong(last_free_index, last_free_index + 1)) {
        while (true) {
          T* data{queue[last_free_index].load()};
          if (data == nullptr) {
            queue[last_free_index].store(ptr);
            queue[last_free_index].notify_one();
            return true;
          }
          queue[last_free_index].wait(data);
        }
      }
    }
  }
  auto mmanager_push(T&& object) -> bool {
    while (true) {
      uint64_t last_free_index = tail.load();
      if ((last_free_index + 1U) == head) {
        return false;
      }
      if (tail.compare_exchange_strong(last_free_index, last_free_index + 1U)) {
        auto* raw_atomic_slot{memory_queue[last_free_index]};
        auto& atomic_slot{
            *std::launder(static_cast<AtomicSlot<T>*>(raw_atomic_slot)),
        };
        while (true) {
          atomic_slot.is_initialized.wait(true);
          if (!atomic_slot.is_initialized.load()) {
            atomic_slot.emplace(object);
            atomic_slot.is_initialized.store(true);
            atomic_slot.is_initialized.notify_one();
            return true;
          }
        }
      }
    }
  }
  auto pop() -> T* {
    uint16_t last_busy_index = head.load();
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
    auto* raw_atomic_slot{memory_queue[last_busy_index]};
    auto& atomic_slot{
        *std::launder(static_cast<AtomicSlot<T>*>(raw_atomic_slot)),
    };
    while (true) {
      atomic_slot.is_initialized.wait(false);
      if (atomic_slot.is_initialized.load()) {
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
