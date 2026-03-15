#include "atomic_queue.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <thread>

namespace bsm {

std::optional<unsigned int> AtomicQueue::pop() {
  for (short i = 0; i < 2; i++) {
    uint8_t last_busy_index = head.load();
    if (last_busy_index == tail)
      return std::nullopt;
    if (head.compare_exchange_strong(last_busy_index, last_busy_index + 1)) {
      AtomicSlot data{};
      while (!data.ready) {
        queue[last_busy_index].wait(data);
        data = queue[last_busy_index].load();
        queue[last_busy_index].compare_exchange_strong(data, {});
      }
      queue[last_busy_index].notify_one();
      return data.slot;
    }
  }
  return std::nullopt;
}

bool AtomicQueue::push(unsigned int slot) {
  while (true) {
    uint8_t last_free_index = tail.load();
    if ((last_free_index + 1) == head)
      return false;
    if (tail.compare_exchange_strong(last_free_index, last_free_index + 1)) {
      while (true) {
        AtomicSlot data{queue[last_free_index].load()};
        if (!data.ready) {
          data.slot = slot;
          data.ready = true;
          queue[last_free_index].exchange(data);
          queue[last_free_index].notify_one();
          return true;
        }
        queue[last_free_index].wait(data);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    continue;
  }
}
} // namespace bsm
