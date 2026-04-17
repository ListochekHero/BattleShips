#ifndef NETWORK_DEFS_H
#define NETWORK_DEFS_H

#include <cstdint>

namespace bsm {

enum class end_point_e : uint8_t {
  NONE = 0,
  CLIENT = 1 << 0,
  LOBBY = 1 << 1,
  PARENT = 1 << 2,
  LISTENER = 1 << 3,
  SERVER = 1 << 4
};

} // namespace bsm

#endif
