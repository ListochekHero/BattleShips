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
  auto normilize_ring_slot(uint64_t ring_slot) -> uint64_t;
  auto try_place_into_queue(uint64_t last_free_slot, uint64_t new_free_index)
      -> bool;

  std::atomic_uint64_t head{0};
  std::atomic_uint64_t tail{0};
  std::atomic_uint64_t* ring_start_ptr{nullptr};

  static constexpr uint64_t RING_BUFFER_SIZE = 4096;
};

struct AllocatorPoolMetaLayout {
  int64_t chunk_size{0};
  int64_t page_size{0};
  std::atomic<char*> actual_memory_end{nullptr};
  std::binary_semaphore page_allocation_permit{1};
  std::atomic_uint32_t last_free_index{0};
  RingBuffer free_indexes_queue{};
};

struct AllocatorPool {
public:
  void init(char* meta_data_ptr, int64_t chunk_size);
  auto allocate() -> std::optional<AllocationResult>;
  void deallocate(uint64_t index_to_free);

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
  auto allocate() -> std::optional<AllocationResult>;
  void deallocate(size_t index_to_free);
  auto operator[](int64_t index) -> void*;

private:
  void init_meta_storage(size_t meta_storage_size);

  char* raw_meta_data_ptr_{nullptr};
};

template <typename T> auto get_meta_data(const char* raw_meta_ptr) -> T& {
  auto* object_ptr{reinterpret_cast<T*>(raw_meta_ptr)};
  return *std::launder(object_ptr);
}

} // namespace bsm

#endif
