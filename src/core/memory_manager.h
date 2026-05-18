#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "utility/error.h"

#include <cstddef>
#include <cstdint>
#include <unistd.h>

#define NODE_SIZE 1

namespace bsm {

class MemoryManager {
public:
  void init(std::uint64_t memory_amount);
  auto allocate_raw() -> void*;
  auto operator[](size_t index) -> void*;

private:
  auto allocate_new_node() -> std::optional<Error>;

  void* virtual_pool_{nullptr};
  char* free_beggins_{nullptr};
  std::uint64_t page_size_{static_cast<uint64_t>(sysconf(_SC_PAGE_SIZE))};
  std::uint32_t object_size_{80};
  std::uint32_t space_left_{0};
  std::uint32_t pages_allocated_{0};
};

} // namespace bsm

#endif
