#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "protocol/memory_manager/memory_manager_types.h"
#include "utility/error.h"

#include <atomic>
#include <cstdint>
#include <semaphore>
#include <unistd.h>

namespace bsm {

#define PAGES_PER_NODE 256
#define BYTES_IN_CHUNK 8
#define BITS_IN_WORD 64

struct MetaStorageLayout {
  std::atomic<char*> free_begins;
  std::atomic<char*> actual_memory_end;
  const int64_t object_size;
  const int64_t page_size;
  int64_t actual_pages_allocated{0};
  std::binary_semaphore page_allocation_permit{1};
  std::atomic_uint64_t bit_map[]; // NOLINT
};

class MemoryManager {
public:
  static auto calculate_memory_amount(int64_t object_size, int64_t object_count)
      -> int64_t;
  void init(PoolInitParam init_param);
  auto allocate_raw() -> AllocationResult;
  auto allocate_sized_raw(size_t size_to_allocate) -> AllocationResult;
  auto deallocate_raw(size_t object_index);
  auto operator[](int64_t index) -> void*;

private:
  auto mark_free(size_t chunks_to_free, int occupied_start_pos, int word_count)
      -> bool;
  auto find_allocation_place(size_t chunks_needed) -> int64_t;
  auto take_place(size_t chunks_needed, int free_start_pos, int word_count)
      -> bool;
  auto allocate_new_node() -> std::optional<Error>;
  void populate_meta_storage(std::int64_t object_size, char* actual_memory_end);
  static auto calc_bytes_chunk(size_t object_size) -> size_t;
  auto get_meta_data() -> MetaStorageLayout&;

  char* virtual_pool_{nullptr};
  char* raw_meta_data_{nullptr};
};

} // namespace bsm

#endif
