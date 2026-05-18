#include "memory_manager.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <cstddef>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>

namespace bsm {

void MemoryManager::init(std::uint64_t memory_amount) {
  virtual_pool_ = mmap(nullptr, memory_amount, PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  free_beggins_ = static_cast<char*>(virtual_pool_);
}
  void MemoryManager::set_object_size(size_t size){

  }

auto MemoryManager::allocate_raw() -> void* {
  if (space_left_ < object_size_) {
    success_or_terminate(allocate_new_node());
  }
  void* object_ptr = free_beggins_;
  free_beggins_ += object_size_;
  space_left_ -= object_size_;
  return object_ptr;
}

auto MemoryManager::operator[](size_t index) -> void* {
  void* object_ptr = static_cast<char*>(virtual_pool_) + (index * object_size_);
  return object_ptr;
}

auto MemoryManager::allocate_new_node() -> std::optional<Error> {
  std::cout << "Allocating new page, pages already allocated: "
            << pages_allocated_ << '\n'
            << std::flush;
  if (auto protect_result = mprotect(
          static_cast<char*>(virtual_pool_) + (pages_allocated_ * page_size_),
          page_size_ * NODE_SIZE, PROT_READ | PROT_WRITE);
      protect_result == -1) {
    return make_error_c(
        "Unable to allocate new memory page: failed to allocate "
        "physical memory with mprotect");
  }
  pages_allocated_ += NODE_SIZE;
  space_left_ += page_size_ * NODE_SIZE;
  return std::nullopt;
}

} // namespace bsm
