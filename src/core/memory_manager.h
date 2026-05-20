#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "utility/error.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <semaphore>
#include <unistd.h>

namespace bsm {

enum meta_storage_layout : uint8_t {
  FREE_BEGGINS = 0,
  ACTUAL_MEMORY_END = 8,
  OBJECT_SIZE = 16,
  PAGE_SIZE = 24,
  ACTUAL_PAGES_ALLOCATED = 32,
  PAGE_ALLOCATION_PERMIT = 40,
  NODE_SIZE = 255,
};

struct AllocationResult {
  void* memory_ptr;
  uint64_t index;
};

class MemoryManager {
public:
  void init(std::uint64_t memory_amount, size_t object_size);
  auto allocate_raw() -> AllocationResult;
  auto operator[](size_t index) -> void*;

private:
  auto allocate_new_node() -> std::optional<Error>;
  void populate_meta_storage(std::uint64_t object_size);
  template <meta_storage_layout Key> auto get_meta_data() -> decltype(auto) {
    if constexpr (Key == meta_storage_layout::FREE_BEGGINS ||
                  Key == meta_storage_layout::ACTUAL_MEMORY_END) {
      auto* raw_ptr{reinterpret_cast<std::atomic<char*>*>(meta_data_ + Key)};
      return *std::launder(raw_ptr);
    } else if constexpr (Key == meta_storage_layout::OBJECT_SIZE) {
      const auto* raw_ptr{reinterpret_cast<const uint64_t*>(meta_data_ + Key)};
      return *std::launder(raw_ptr);
    } else if constexpr (Key == meta_storage_layout::PAGE_SIZE) {
      const auto* raw_ptr{reinterpret_cast<const uint64_t*>(meta_data_ + Key)};
      return *std::launder(raw_ptr);
    } else if constexpr (Key == meta_storage_layout::ACTUAL_PAGES_ALLOCATED) {
      auto* raw_ptr{reinterpret_cast<std::atomic_uint64_t*>(meta_data_ + Key)};
      return *std::launder(raw_ptr);
    } else if constexpr (Key == meta_storage_layout::PAGE_ALLOCATION_PERMIT) {
      auto* raw_ptr{reinterpret_cast<std::binary_semaphore*>(meta_data_ + Key)};
      return *std::launder(raw_ptr);
    } else if constexpr (Key == meta_storage_layout::NODE_SIZE) {
      static_assert(Key != meta_storage_layout::NODE_SIZE,
                    "NODE_SIZE is not stored in meta_data_");
    } else {
      static_assert(Key == meta_storage_layout::FREE_BEGGINS,
                    "Unsupported meta storage key");
    }
  }

  char* virtual_pool_{nullptr};
  char* meta_data_{nullptr};
};

} // namespace bsm

#endif
