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

struct DeliveryReport {
  size_t delivered{0};
  size_t failed{0};
};

struct RegistrationReport {
  size_t registered{0};
  size_t failed{0};
};

struct TransferResult {
  bool destination_conn_preserved{true};
  bool source_conn_preserved{true};
};

} // namespace bsm

#endif
