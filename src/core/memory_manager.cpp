#include "memory_manager.h"

#include "protocol/memory_manager/memory_manager_types.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <limits>
#include <new>
#include <semaphore>
#include <sys/mman.h>
#include <unistd.h>

namespace bsm {

void RingBuffer::init(char* buffer_start_ptr) {
  std::atomic_uint64_t* ring_buffer{
      reinterpret_cast<std::atomic_uint64_t*>(buffer_start_ptr)};
  for (int i = 0; i < 4096; ++i) {
    new (ring_buffer + i)
        std::atomic_uint64_t{std::numeric_limits<uint64_t>::max()};
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
        if (ring_slot.load() != max_value) {
          uint64_t free_index{ring_slot.exchange(max_value)};
          ring_slot.notify_one();
          return free_index;
        }
        ring_slot.wait(max_value);
      }
    }
  }
}

auto RingBuffer::try_push(uint64_t new_free_index) -> bool {
  while (true) {
    uint64_t last_free_slot{head.load()};
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

auto RingBuffer::normilize_ring_slot(uint64_t ring_slot) -> uint64_t {
  constexpr uint64_t RING_BUFFER_SIZE = 4096;
  constexpr uint64_t RING_MASK = RING_BUFFER_SIZE - 1;
  return ring_slot & RING_MASK;
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

void AllocatorPool::init(char* meta_data_ptr, int64_t chunk_size) {
  raw_meta_data_prt_ = meta_data_ptr;
  virtual_pool_ptr_ =
      static_cast<char*>(mmap(nullptr, 1024 * 1024 * 1024 * 10L, PROT_NONE,
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
  uint32_t new_index{meta_data.last_not_issued_index.load()};
  while (true) {
    if (new_index >= 4095) {
      return std::nullopt;
    }
    char* next_object_ptr{
        virtual_pool_ptr_ + ((new_index + 1) * meta_data.chunk_size),
    };
    char* current_memory_end{meta_data.actual_memory_end.load()};
    if (next_object_ptr >= current_memory_end) {
      if (meta_data.page_allocation_permit.try_acquire()) {
        success_or_terminate(allocate_new_node());
        meta_data.page_allocation_permit.release();
      } else {
        meta_data.actual_memory_end.wait(current_memory_end);
      }
    }
    if (meta_data.last_not_issued_index.compare_exchange_weak(new_index,
                                                              new_index + 1)) {
      void* object_ptr{virtual_pool_ptr_ + (new_index * meta_data.chunk_size)};
      return AllocationResult{.memory_ptr = object_ptr, .index = new_index};
    }
  }
}

void AllocatorPool::deallocate(uint64_t index_to_free) {
  auto& meta_data{get_meta_data<AllocatorPoolMetaLayout>(raw_meta_data_prt_)};
  if (!meta_data.free_indexes_queue.try_push(index_to_free)) {
    meta_data.free_indexes_queue.push(index_to_free);
  }
}

auto AllocatorPool::operator[](int64_t index) -> void* {
  void* object_ptr =
      virtual_pool_ptr_ +
      (index *
       get_meta_data<AllocatorPoolMetaLayout>(raw_meta_data_prt_).chunk_size);
  return object_ptr;
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

auto AllocatorPool::allocate_new_node() -> std::optional<Error> {
  auto& meta_struct{get_meta_data<AllocatorPoolMetaLayout>(raw_meta_data_prt_)};
  int64_t memory_amount{meta_struct.page_size * PAGES_PER_NODE};
  if (auto protect_result = mprotect(meta_struct.actual_memory_end,
                                     static_cast<uint64_t>(memory_amount),
                                     PROT_READ | PROT_WRITE);
      protect_result == -1) {
    return make_error_c(
        "Unable to allocate new memory page: failed to allocate "
        "physical memory with mprotect");
  }
  meta_struct.actual_memory_end.fetch_add(memory_amount);
  meta_struct.actual_memory_end.notify_all();
  return std::nullopt;
}

void MemoryManager::init() {
  constexpr uint64_t RING_BUFFER_SIZE = 4096;
  size_t meta_storage_size{
      sizeof(ManagerMetaLayout) + sizeof(AllocatorPoolMetaLayout) +
          (RING_BUFFER_SIZE * sizeof(std::atomic_uint64_t)),
  };
  int64_t page_size{sysconf(_SC_PAGE_SIZE)};
  size_t remainder{meta_storage_size % page_size};
  if (remainder != 0) {
    meta_storage_size += page_size - remainder;
  }
  init_meta_storage(meta_storage_size);
}

auto MemoryManager::allocate() -> std::optional<AllocationResult> {
  return get_meta_data<ManagerMetaLayout>(raw_meta_data_ptr_)
      .memory_pool_512_.allocate();
}

void MemoryManager::deallocate(size_t index_to_free) {
  get_meta_data<ManagerMetaLayout>(raw_meta_data_ptr_)
      .memory_pool_512_.deallocate(index_to_free);
}

auto MemoryManager::operator[](int64_t index) -> void* {
  return get_meta_data<ManagerMetaLayout>(raw_meta_data_ptr_)
      .memory_pool_512_[index];
}

void MemoryManager::init_meta_storage(size_t meta_storage_size) {
  raw_meta_data_ptr_ = static_cast<char*>(
      mmap(nullptr, meta_storage_size, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  auto* meta{new (raw_meta_data_ptr_) ManagerMetaLayout()};
  meta->memory_pool_512_.init(reinterpret_cast<char*>(meta + 1), 512);
}

} // namespace bsm
