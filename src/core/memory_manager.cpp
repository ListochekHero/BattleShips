#include "memory_manager.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <new>
#include <semaphore>
#include <sys/mman.h>
#include <unistd.h>

namespace bsm {

auto calculate_memory_amount(int64_t object_size, int64_t object_count)
    -> int64_t {
  const int64_t page_size{sysconf(_SC_PAGE_SIZE)};
  int64_t memory_required{object_size * object_count};
  int64_t remainder{memory_required % page_size};
  return memory_required + (page_size - remainder);
}

void MemoryManager::init(PoolInitParam init_param) {
  int prot_flags;
  if (init_param.pool_type == PoolType::DYNAMIC) {
    prot_flags = PROT_NONE;
  } else {
    prot_flags = PROT_READ | PROT_WRITE;
  }
  virtual_pool_ =
      static_cast<char*>(mmap(nullptr, init_param.memory_amount, prot_flags,
                                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  char* memory_end = virtual_pool_;
  if (init_param.pool_type == PoolType::STATIC) {
    memory_end = virtual_pool_ + init_param.memory_amount - 1;
  }
  populate_meta_storage(init_param.object_size, memory_end);
}

auto MemoryManager::allocate_raw() -> AllocationResult {
  auto& free_beggins_atomic{get_meta_data<meta_storage_layout::FREE_BEGGINS>()};
  auto& memory_end_atomic{
      get_meta_data<meta_storage_layout::ACTUAL_MEMORY_END>(),
  };
  const uint64_t object_size{
      get_meta_data<meta_storage_layout::OBJECT_SIZE>(),
  };
  auto& page_allocation_permit{
      get_meta_data<meta_storage_layout::PAGE_ALLOCATION_PERMIT>()};
  char* object_ptr{free_beggins_atomic.load()};
  while (true) {
    char* next_object_ptr{object_ptr + object_size};
    char* current_memory_end{memory_end_atomic.load()};
    if (next_object_ptr > current_memory_end) {
      if (page_allocation_permit.try_acquire()) {
        allocate_new_node();
        page_allocation_permit.release();
      } else {
        memory_end_atomic.wait(current_memory_end);
      }
    }
    if (free_beggins_atomic.compare_exchange_weak(object_ptr,
                                                  next_object_ptr)) {
      break;
    }
  }
  uint64_t index{
      static_cast<uint64_t>((object_ptr - virtual_pool_) / object_size)};
  return {.memory_ptr = static_cast<void*>(object_ptr), .index = index};
}

auto MemoryManager::operator[](size_t index) -> void* {
  void* object_ptr =
      virtual_pool_ +
      (index * get_meta_data<meta_storage_layout::OBJECT_SIZE>());
  return object_ptr;
}

auto MemoryManager::allocate_new_node() -> std::optional<Error> {
  auto& pages_allocated_atomic{
      get_meta_data<meta_storage_layout::ACTUAL_PAGES_ALLOCATED>()};
  const auto& page_size{get_meta_data<meta_storage_layout::PAGE_SIZE>()};

  if (auto protect_result = mprotect(
          virtual_pool_ + (pages_allocated_atomic.load() * page_size),
          page_size * meta_storage_layout::NODE_SIZE, PROT_READ | PROT_WRITE);
      protect_result == -1) {
    return make_error_c(
        "Unable to allocate new memory page: failed to allocate "
        "physical memory with mprotect");
  }
  pages_allocated_atomic.fetch_add(NODE_SIZE);
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
