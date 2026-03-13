#ifndef ATOMIC_QUEUE_H
#define ATOMIC_QUEUE_H

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace bsm {

class AtomicQueue {
public:
  size_t pop();
  bool push(size_t slot);

private:
  std::atomic_uint8_t head{0};
  std::atomic_uint8_t tail{0};
  std::array<std::atomic_size_t, std::numeric_limits<uint8_t>::max()> queue;
};

} // namespace bsm
#endif
