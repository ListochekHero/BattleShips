#include "memory_manager.h"

#include "protocol/memory_manager/memory_manager_types.h"
#include "utility/error.h"

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <new>
#include <semaphore>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

namespace bsm {

void RingBuffer::init(char* buffer_start_ptr) {
  std::atomic_uint64_t* ring_buffer{
      reinterpret_cast<std::atomic_uint64_t*>(buffer_start_ptr)};
  for (int i = 0; i < 4096; ++i) {
    new (ring_buffer + i) std::atomic_uint64_t{0};
  }
  ring_start_ptr = ring_buffer;
}

auto RingBuffer::try_pop() -> std::optional<uint64_t> {
  uint64_t last_busy_slot{tail.load()};
  while (true) {
    if (last_busy_slot == head.load()) {
      return std::nullopt;
    }
    if (tail.compare_exchange_weak(last_busy_slot, last_busy_slot + 1)) {
      uint64_t normalized_ring_slot{normilize_ring_slot(last_busy_slot)};
      std::atomic_uint64_t& ring_slot{*(ring_start_ptr + normalized_ring_slot)};
      uint64_t max_value{std::numeric_limits<uint64_t>::max()};
      while (true) {
        ring_slot.wait(max_value);
        if (ring_slot.load() != max_value) {
          uint64_t free_index{ring_slot.exchange(max_value)};
          ring_slot.notify_one();
          return free_index;
        }
      }
    }
  }
}

auto RingBuffer::try_push(uint64_t new_free_index) -> bool {
  uint64_t last_free_slot{head.load()};
  while (true) {
    if (last_free_slot + 1 == tail) {
      return false;
    }
    if (try_place_into_queue(last_free_slot, new_free_index)) {
      return true;
    }
  }
}

auto RingBuffer::push(uint64_t new_free_index) -> void {
  while (true) {
    uint64_t last_free_slot{0};
    while (true) {
      last_free_slot = head.load();
      if (last_free_slot + 1 == tail) {
        head.wait(last_free_slot);
      } else {
        break;
      }
    }
    if (try_place_into_queue(last_free_slot, new_free_index)) {
      return;
    }
  }
}

auto RingBuffer::try_place_into_queue(uint64_t last_free_slot,
                                      uint64_t new_free_index) -> bool {
  if (head.compare_exchange_weak(last_free_slot, last_free_slot + 1)) {
    uint64_t normalized_ring_slot{normilize_ring_slot(last_free_slot)};
    std::atomic_uint64_t& ring_slot{*(ring_start_ptr + normalized_ring_slot)};
    while (true) {
      uint64_t previous_data{ring_slot.load()};
      if (previous_data == std::numeric_limits<uint64_t>::max()) {
        ring_slot.store(new_free_index);
        ring_slot.notify_one();
        return true;
      }
      ring_slot.wait(previous_data);
    }
  }
  return false;
}

auto RingBuffer::normilize_ring_slot(uint64_t ring_slot) -> uint64_t {
  constexpr uint64_t RING_BUFFER_SIZE = 4096;
  constexpr uint64_t RING_MASK = RING_BUFFER_SIZE - 1;
  return ring_slot & RING_MASK;
}

void AllocatorPool::init(char* meta_data_ptr, int64_t chunk_size) {
  raw_meta_data_prt_ = meta_data_ptr;
  virtual_pool_ptr_ =
      static_cast<char*>(mmap(nullptr, 1024 * 1024 * 1024 * 10, PROT_NONE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  init_meta_storage(chunk_size);
}

auto AllocatorPool::allocate() -> std::optional<AllocationResult> {
  auto& meta_data{get_meta_data<AllocatorPoolMetaLayout>(raw_meta_data_prt_)};
  auto free_index{meta_data.free_indexes_queue.try_pop()};
  if (free_index) {
    void* object_ptr{virtual_pool_ptr_ +
                     ((*free_index) * meta_data.chunk_size)};
    return AllocationResult{.memory_ptr = object_ptr, .index = *free_index};
  }
  uint32_t actual_last_index{meta_data.last_free_index.load()};
  while (true) {
    if (actual_last_index >= 4095) {
      return std::nullopt;
    }
    if (meta_data.last_free_index.compare_exchange_weak(
            actual_last_index, actual_last_index + 1)) {
      void* object_ptr{virtual_pool_ptr_ +
                       (actual_last_index * meta_data.chunk_size)};
      return AllocationResult{.memory_ptr = object_ptr,
                              .index = actual_last_index};
    }
  }
}

void AllocatorPool::deallocate(uint64_t index_to_free) {
  auto& meta_data{get_meta_data<AllocatorPoolMetaLayout>(raw_meta_data_prt_)};
  meta_data.free_indexes_queue.try_push(index_to_free);
}

void AllocatorPool::init_meta_storage(int64_t chunk_size) {
  auto* meta{
      new (raw_meta_data_prt_) AllocatorPoolMetaLayout{
          .chunk_size = chunk_size,
          .page_size = sysconf(_SC_PAGE_SIZE),
          .actual_memory_end = virtual_pool_ptr_,
      },
  };
  meta->free_indexes_queue.init(reinterpret_cast<char*>(meta + 1));
}

auto MemoryManager::calculate_memory_amount(int64_t object_size,
                                            int64_t object_count) -> int64_t {
  const int64_t page_size{sysconf(_SC_PAGE_SIZE)};
  int64_t memory_required{object_size * object_count};
  int64_t remainder{memory_required % page_size};
  return memory_required + (page_size - remainder);
}

void MemoryManager::init(PoolInitParam init_param) {
  constexpr uint64_t RING_BUFFER_SIZE = 4096;
  size_t meta_storage_size{sizeof(ManagerMetaLayout) +
                           sizeof(AllocatorPoolMetaLayout) +
                           RING_BUFFER_SIZE * sizeof(int32_t)};
  int64_t page_size{sysconf(_SC_PAGE_SIZE)};
  size_t remainder{meta_storage_size % page_size};
  if (remainder != 0) {
    meta_storage_size += page_size - remainder;
  }
  init_meta_storage(meta_storage_size);
  }

void MemoryManager::init_meta_storage(size_t meta_storage_size) {
  raw_meta_data_ptr_ = static_cast<char*>(
      mmap(nullptr, meta_storage_size, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  auto* meta{new (raw_meta_data_ptr_) ManagerMetaLayout()};
  meta->memory_pool_512_.init(reinterpret_cast<char*>(meta + 1), 512);
}

auto MemoryManager::allocate() -> std::optional<AllocationResult> {
  return get_meta_data<ManagerMetaLayout>(raw_meta_data_ptr_)
      .memory_pool_512_.allocate();
}

void MemoryManager::deallocate(size_t index_to_free) {
  return get_meta_data<ManagerMetaLayout>(raw_meta_data_ptr_)
      .memory_pool_512_.deallocate(index_to_free);
}

auto MemoryManager::allocate_old() -> AllocationResult {

  auto& meta_struct{get_meta_data<ManagerMetaLayout>(raw_meta_data_ptr_)};
  meta_struct.memory_pool_512_.allocate();

  char* object_ptr{meta_struct.free_begins.load()};
  while (true) {
    char* next_object_ptr{object_ptr + meta_struct.object_size};
    char* current_memory_end{meta_struct.actual_memory_end.load()};
    if (next_object_ptr > current_memory_end) {
      if (meta_struct.page_allocation_permit.try_acquire()) {
        allocate_new_node();
        meta_struct.page_allocation_permit.release();
      } else {
        meta_struct.actual_memory_end.wait(current_memory_end);
      }
    }
    if (meta_struct.free_begins.compare_exchange_weak(object_ptr,
                                                      next_object_ptr)) {
      break;
    }
  }
  uint64_t index{
      static_cast<uint64_t>((object_ptr - virtual_pool_) /
                            meta_struct.object_size),
  };
  return {.memory_ptr = static_cast<void*>(object_ptr), .index = index};
}

auto MemoryManager::operator[](int64_t index) -> void* {
  void* object_ptr = virtual_pool_ + (index * get_meta_data().object_size);
  return object_ptr;
}

auto MemoryManager::allocate_sized_raw(size_t size_to_allocate)
    -> AllocationResult {
  size_t byte_chunks_required{calc_bytes_chunk(size_to_allocate)};
  int64_t meta_start_pos{
      find_allocation_place(byte_chunks_required + 1),
  }; // +1 is for meta data about size of object
  char* object_meta_ptr{virtual_pool_ + (meta_start_pos * BYTES_IN_CHUNK)};
  new (object_meta_ptr) uint64_t{byte_chunks_required};
  int64_t object_start_pos{meta_start_pos + 1};
  return {
      .memory_ptr = virtual_pool_ + (object_start_pos * BYTES_IN_CHUNK),
      .index = static_cast<uint64_t>(object_start_pos),
  };
}

auto MemoryManager::deallocate_raw(size_t object_index) {
  uint64_t meta_start_pos{object_index - 1};
  char* meta_start_ptr{virtual_pool_ + (meta_start_pos * BYTES_IN_CHUNK)};
  uint64_t occupied_chunks{
      *std::launder(reinterpret_cast<uint64_t*>(meta_start_ptr)),
  };
  mark_free(occupied_chunks, meta_start_pos % BYTES_IN_CHUNK,
            meta_start_pos / BYTES_IN_CHUNK);
}

auto MemoryManager::calc_bytes_chunk(size_t object_size) -> size_t {
  return (object_size + BYTES_IN_CHUNK - 1) / BYTES_IN_CHUNK;
}

auto MemoryManager::find_allocation_place(size_t chunks_needed) -> int64_t {
  auto& meta_struct{get_meta_data()};
  int word_count{0};
  int carry_over_free_chunks{0};
  int carry_over_index{0};
  while (true) {
    uint64_t inverted_bit_map{~meta_struct.bit_map[word_count]};
    carry_over_free_chunks += std::countr_one(inverted_bit_map);
    if (std::cmp_greater_equal(carry_over_free_chunks, chunks_needed)) {
      if (take_place(chunks_needed, carry_over_index, word_count)) {
        return static_cast<int64_t>(carry_over_index);
      }
    }
    uint64_t candidates{};
    for (size_t i = 0; i < chunks_needed; i++) {
      candidates &= inverted_bit_map >> i;
    }
    if (candidates != 0) {
      int free_chunks_start{std::countr_zero(candidates)};
      if (take_place(chunks_needed, free_chunks_start, word_count)) {
        return (word_count * BITS_IN_WORD) + free_chunks_start;
      }
    }
    carry_over_free_chunks = std::countl_one(inverted_bit_map);
    carry_over_index =
        BITS_IN_WORD - carry_over_free_chunks + (word_count * BITS_IN_WORD);
  }
}

auto MemoryManager::take_place(size_t chunks_needed, int free_start_pos,
                               int word_count) -> bool {
  uint64_t bit_mask{(1ULL << chunks_needed) - 1};
  bit_mask <<= free_start_pos;
  auto& meta_struct{get_meta_data()};
  uint64_t current_word{meta_struct.bit_map[word_count]};
  if ((~current_word & bit_mask) != bit_mask) {
    return false;
  }
  uint64_t desired_word{current_word | bit_mask};
  return meta_struct.bit_map[word_count].compare_exchange_strong(current_word,
                                                                 desired_word);
}

auto MemoryManager::mark_free(size_t chunks_to_free, int occupied_start_pos,
                              int word_count) -> bool {
  uint64_t bit_mask{(1ULL << chunks_to_free) - 1};
  bit_mask <<= occupied_start_pos;
  auto& meta_struct{get_meta_data()};
  uint64_t current_word{meta_struct.bit_map[word_count]};
  if ((current_word & bit_mask) != bit_mask) {
    return false;
  }
  uint64_t desired_word{current_word ^ bit_mask};
  return meta_struct.bit_map[word_count].compare_exchange_strong(current_word,
                                                                 desired_word);
}

auto MemoryManager::allocate_new_node() -> std::optional<Error> {
  auto& meta_struct{get_meta_data()};
  int64_t memory_amount{meta_struct.page_size * PAGES_PER_NODE};
  if (auto protect_result = mprotect(
          virtual_pool_ +
              (meta_struct.actual_pages_allocated * meta_struct.page_size),
          static_cast<uint64_t>(memory_amount), PROT_READ | PROT_WRITE);
      protect_result == -1) {
    return make_error_c(
        "Unable to allocate new memory page: failed to allocate "
        "physical memory with mprotect");
  }
  meta_struct.actual_memory_end.fetch_add(memory_amount);
  meta_struct.actual_memory_end.notify_all();
  meta_struct.actual_pages_allocated += (PAGES_PER_NODE);
  return std::nullopt;
}

void MemoryManager::populate_meta_storage(std::int64_t object_size,
                                          char* actual_memory_end) {
  int64_t page_size{sysconf(_SC_PAGE_SIZE)};
  raw_meta_data_ = static_cast<char*>(
      mmap(nullptr, static_cast<uint64_t>(page_size), PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  auto* meta{
      new (raw_meta_data_) MetaStorageLayout{
          .free_begins = virtual_pool_,
          .actual_memory_end = actual_memory_end,
          .object_size = object_size,
          .page_size = page_size,
          .actual_pages_allocated = 0,
      },
  };
}

auto MemoryManager::get_meta_data() -> MetaStorageLayout& {
  auto* raw_ptr{reinterpret_cast<MetaStorageLayout*>(raw_meta_data_)};
  return *std::launder(raw_ptr);
}

} // namespace bsm
