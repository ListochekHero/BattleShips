#ifndef MEMORY_MANAGER_DEFS_H
#define MEMORY_MANAGER_DEFS_H

#include <cstdint>

namespace bsm {

enum class PoolType : uint8_t {
  DYNAMIC,
  STATIC,
};

} // namespace bsm

#endif
