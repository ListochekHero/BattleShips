#ifndef VIRTUAL_MMANAGER_H
#define VIRTUAL_MMANAGER_H

#include "utility/error.h"

#include <cstdint>
#include <unistd.h>

namespace bsm {

class Virtual_MManager {
public:
  void init(std::uint64_t memory_amount);
  auto allocate_raw() -> void*;
  auto operator[](size_t index) -> void*;

private:
  auto allocate_new_page() -> std::optional<Error>;

  void* virtual_pool_{nullptr};
  char* free_beggins_{nullptr};
  std::uint64_t page_size_{static_cast<uint64_t>(sysconf(_SC_PAGE_SIZE))};
  std::uint32_t object_size_{80};
  std::uint32_t space_left_{0};
  std::uint32_t pages_allocated_{0};
};

} // namespace bsm

#endif
