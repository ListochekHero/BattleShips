#ifndef NETWORK_TYPES_H
#define NETWORK_TYPES_H

#include <cstddef>
#include <limits>

namespace bsm {

class ConnectionView {
public:
  explicit ConnectionView() : slot(std::numeric_limits<size_t>::max()) {};
  explicit ConnectionView(size_t slot);
  [[nodiscard]] auto get_slot() const -> size_t;

private:
  size_t slot{std::numeric_limits<std::size_t>::max()};
};

} // namespace bsm

#endif
