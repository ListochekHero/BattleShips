#ifndef MEMORY_MANAGER_TYPES_H
#define MEMORY_MANAGER_TYPES_H

#include "protocol/memory_manager/memory_manager_defs.h"
#include <cstdint>

namespace bsm {

struct PoolInitParam {
  int64_t memory_amount;
  int64_t object_size;
  PoolType pool_type;
};

struct AllocationResult {
  void* memory_ptr;
  uint64_t index;
};

} // namespace bsm

#endif
