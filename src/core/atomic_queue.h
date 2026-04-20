#ifndef ATOMIC_QUEUE_H
#define ATOMIC_QUEUE_H

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>

namespace bsm {

struct AtomicSlot {
  bool ready{false};
  unsigned int slot{std::numeric_limits<int>::max()};
};

template <typename T> class AtomicQueue {
public:
  // AtomicQueue() { queue.fill(nullptr); }

  T* try_pop() {
    uint8_t last_busy_index = head.load();
    if (last_busy_index == tail)
      return nullptr;
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

  bool push(T* ptr) {
    while (true) {
      uint8_t last_free_index = tail.load();
      if ((last_free_index + 1) == head)
        return false;
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
      continue;
    }
  }

  T* pop() {
    uint8_t last_busy_index = head.load();
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
    uint8_t last_busy_index = head.load();
    T* data{nullptr};
    while (data == nullptr) {
      queue[last_busy_index].wait(data);
      if(queue[last_busy_index]!=nullptr)break;
    }
  }

private:
  std::atomic_uint8_t head{0};
  std::atomic_uint8_t tail{0};
  std::array<std::atomic<T*>, std::numeric_limits<uint16_t>::max()> queue{};
};

} // namespace bsm
#endif
