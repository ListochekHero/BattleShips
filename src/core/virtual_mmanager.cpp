#include "virtual_mmanager.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <cstddef>
#include <sys/mman.h>
#include <unistd.h>

namespace bsm {

void Virtual_MManager::init(std::uint64_t memory_amount) {
  std::uint64_t number_of_pages{memory_amount / page_size_};
  virtual_pool_ =
      mmap(nullptr, number_of_pages, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
  free_beggins_ = static_cast<char*>(virtual_pool_);
}

auto Virtual_MManager::allocate_raw() -> void* {
  if (space_left_ < object_size_) {
    success_or_terminate(allocate_new_page());
  }
  void* object_ptr = free_beggins_;
  free_beggins_ += object_size_;
  space_left_ -= object_size_;
  return object_ptr;
}

auto Virtual_MManager::operator[](size_t index) -> void* {
  void* object_ptr = static_cast<char*>(virtual_pool_) + (index * object_size_);
  return object_ptr;
}

auto Virtual_MManager::allocate_new_page() -> std::optional<Error> {
  if (auto protect_result = mprotect(static_cast<char*>(virtual_pool_) +
                                         (pages_allocated_ * page_size_),
                                     page_size_, PROT_READ | PROT_WRITE);
      protect_result == -1) {
    return make_error_c(
        "Unable to allocate new memory page: failed to allocate "
        "physical memory with mprotect");
  }
  pages_allocated_++;
  space_left_ += page_size_;
  return std::nullopt;
}

} // namespace bsm
