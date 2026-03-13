#include "atomic_queue.h"
#include <cstddef>
#include <cstdint>

namespace bsm {

size_t AtomicQueue::pop() {
  uint8_t last_busy_index = head.load();
  if (queue[last_busy_index] == 0)
    return 0;
  if (head.compare_exchange_strong(last_busy_index, last_busy_index + 1)) {
    queue[last_busy_index].wait(0);
    size_t slot = queue[last_busy_index].load();
    queue[last_busy_index] = 0;
    return slot;
  }
  return 0;
}

bool AtomicQueue::push(size_t slot) {
  while (true) {
    uint8_t last_free_index = tail.load();
    if ((last_free_index + 1) == head)
      return false;
    if (tail.compare_exchange_strong(last_free_index, last_free_index + 1)) {
      queue[last_free_index].exchange(slot);
      queue[last_free_index].notify_one();
      return true;
    }
    continue;
  }
}

} // namespace bsm
