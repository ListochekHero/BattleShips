#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

// IWYU pragma: no_include <string>
#include "core/atomic_queue.h"
#include "core/memory_manager.h"
#include "utility/error.h"
#include "utility/logger.h"
#include "utility/utility.h"
#include <cstddef>
#include <exception>
#include <expected>
#include <iostream>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace bsm {

template <typename T> class ObjectPool {
public:
  ObjectPool() { mmanager_.init(); };
  auto get_object(size_t slot_index) -> T& {
    return *static_cast<T*>(mmanager_[slot_index]);
  }
  auto operator[](size_t slot_index) -> T& {
    return *static_cast<T*>(mmanager_[slot_index]);
  }

  auto push_to_pool(T&& object) -> std::expected<size_t, Error> {
    T* object_ptr;
    auto empty_slot(available_slots_.try_pop());
    if (empty_slot.has_value()) {
      void* raw_ptr{mmanager_[*empty_slot]};
      auto* old_object{static_cast<T*>(raw_ptr)};
      *old_object = std::move(object);
      return *empty_slot;
    }
    auto allocation_result{mmanager_.allocate()};
    if (allocation_result) {
      object_ptr = new (allocation_result->memory_ptr) T(std::move(object));
    }
    counter++;
    return allocation_result->index;
  }

  void release(size_t slot) {
    auto* available_slot{new (std::nothrow) size_t(slot)};
    if (available_slot == nullptr) {
      plain_terminate();
    }
    available_slots_.try_push(slot);
  }
  size_t counter{0};

private:
  MemoryManager mmanager_;
  AtomicQueue<size_t> available_slots_;
};

} // namespace bsm

#endif
