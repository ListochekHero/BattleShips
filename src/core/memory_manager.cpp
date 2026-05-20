#include "memory_manager.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <new>
#include <sys/mman.h>
#include <unistd.h>

namespace bsm {

void MemoryManager::init(std::uint64_t memory_amount,
                         std::uint64_t object_size) {
  meta_data_ = static_cast<char*>(mmap(nullptr, sysconf(_SC_PAGE_SIZE),
                                       PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  populate_meta_storage(object_size);
  virtual_pool_ = static_cast<char*>(mmap(nullptr, memory_amount, PROT_NONE,
                                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  get_meta_data<meta_storage_layout::FREE_BEGGINS>().store(virtual_pool_);
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
  void* object_ptr = virtual_pool_ + (index * object_size_);
  return object_ptr;
}

auto MemoryManager::allocate_new_node() -> std::optional<Error> {
  std::cout << "Allocating new page, pages already allocated: "
            << pages_allocated_ << '\n'
            << std::flush;
  if (auto protect_result = mprotect(
          virtual_pool_ + (pages_allocated_ * page_size_),
          page_size_ * meta_storage_layout::NODE_SIZE, PROT_READ | PROT_WRITE);
      protect_result == -1) {
    return make_error_c(
        "Unable to allocate new memory page: failed to allocate "
        "physical memory with mprotect");
  }
  pages_allocated_ += NODE_SIZE;
  space_left_ += page_size_ * NODE_SIZE;
  return std::nullopt;
}

void MemoryManager::populate_meta_storage(std::uint64_t object_size) {
  new (meta_data_ + meta_storage_layout::FREE_BEGGINS)
      std::atomic<char*>(virtual_pool_);
  new (meta_data_ + meta_storage_layout::ACTUAL_MEMORY_END)
      std::atomic<char*>(virtual_pool_);
  new (meta_data_ + meta_storage_layout::OBJECT_SIZE)
      const uint64_t(object_size);
  new (meta_data_ + meta_storage_layout::PAGE_SIZE)
      const uint64_t(sysconf(_SC_PAGE_SIZE));
  new (meta_data_ + meta_storage_layout::ACTUAL_PAGES_ALLOCATED)
      std::atomic_uint64_t(0);
  new (meta_data_ + meta_storage_layout::PAGE_ALLOCATION_PERMIT)
      std::binary_semaphore(1);
}

} // namespace bsm
