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

enum class BitMapAction : uint8_t {
  MARK_AS_FREE,
  MARK_AS_BUSY,
};

struct RingBuffer {
public:
  void init(char* buffer_start_ptr);
  auto try_pop() -> std::optional<uint64_t>;
  auto try_push(uint64_t new_free_index) -> bool;
  auto push(uint64_t new_free_index) -> void;

private:
  std::atomic_uint64_t head{0};
  std::atomic_uint64_t tail{0};
  char* ring_buffer_ptr{nullptr};
};

struct AllocatorPoolMetaLayout {
  int64_t chunk_size{0};
  int64_t page_size{0};
  std::atomic<char*> actual_memory_end{nullptr};
  std::binary_semaphore page_allocation_permit{1};
  std::atomic_uint8_t last_free_index{0};
  RingBuffer free_indexes_queue{};
};

struct AllocatorPool {
public:
  void init(char* meta_data_ptr, int64_t chunk_size);

private:
  void init_meta_storage(int64_t chunk_size);

  char* raw_meta_data_prt_{nullptr};
  char* virtual_pool_ptr_{nullptr};
};

struct ManagerMetaLayout {
  AllocatorPool memory_pool_512_{};
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
