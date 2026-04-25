#ifndef NETWORK_TYPES_H
#define NETWORK_TYPES_H

#include <cstddef>
#include <limits>

namespace bsm {

class ConnectionView {
public:
  explicit ConnectionView() : slot_(std::numeric_limits<size_t>::max()) {};
  explicit ConnectionView(size_t slot) : slot_{slot} {};
  [[nodiscard]] auto get_slot() const -> size_t { return slot_; };

private:
  size_t slot_{std::numeric_limits<std::size_t>::max()};
};

} // namespace bsm

#endif
