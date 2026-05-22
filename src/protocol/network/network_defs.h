#ifndef NETWORK_DEFS_H
#define NETWORK_DEFS_H

#include <cstdint>

namespace bsm {

enum class end_point_e : uint8_t {
  NONE = 0,
  TO_CLIENT = 1 << 0,
  TO_LOBBY = 1 << 1,
  TO_PARENT = 1 << 2,
  LISTENER = 1 << 3,
  TO_SERVER = 1 << 4,
};

} // namespace bsm

#endif
