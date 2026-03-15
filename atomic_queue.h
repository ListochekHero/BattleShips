#ifndef ATOMIC_QUEUE_H
#define ATOMIC_QUEUE_H

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>

namespace bsm {

struct AtomicSlot {
  bool ready{false};
  unsigned int slot{std::numeric_limits<int>::max()};
};

class AtomicQueue {
public:
  std::optional<unsigned int> pop();
  bool push(unsigned int slot);

private:
  std::atomic_uint8_t head{0};
  std::atomic_uint8_t tail{0};
  std::array<std::atomic<AtomicSlot>, std::numeric_limits<uint8_t>::max()>
      queue{};
};

} // namespace bsm
#endif
