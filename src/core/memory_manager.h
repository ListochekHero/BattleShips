#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "protocol/memory_manager/memory_manager_types.h"
#include "utility/error.h"

#include <atomic>
#include <cstdint>
#include <semaphore>
#include <unistd.h>

namespace bsm {

#define NODE_SIZE 256

struct MetaStorageLayout {
  std::atomic<char*> free_begins;
  std::atomic<char*> actual_memory_end;
  const int64_t object_size;
  const int64_t page_size;
  int64_t actual_pages_allocated{0};
  std::binary_semaphore page_allocation_permit{1};
};

class MemoryManager {
public:
  static auto calculate_memory_amount(int64_t object_size, int64_t object_count)
      -> int64_t;
  void init(PoolInitParam init_param);
  auto allocate_raw() -> AllocationResult;
  auto operator[](int64_t index) -> void*;

private:
  auto allocate_new_node() -> std::optional<Error>;
  void populate_meta_storage(std::int64_t object_size, char* actual_memory_end);
  auto get_meta_data() -> MetaStorageLayout&;

  char* virtual_pool_{nullptr};
  char* raw_meta_data_{nullptr};
};

} // namespace bsm

#endif
