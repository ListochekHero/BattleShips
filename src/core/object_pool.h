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
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace bsm {

template <typename T> class ObjectPool {
public:
  ObjectPool() { mmanager_.init(); };
  auto get_object(size_t slot_index) -> T* { return object_pool_[slot_index]; }
  auto get_object_from_manager(size_t slot_index) -> T& {
    return *static_cast<T*>(mmanager_[slot_index]);
  }
  auto operator[](size_t slot_index) -> T& {
    return *static_cast<T*>(mmanager_[slot_index]);
    // auto* object_ptr = static_cast<T*>(object_pool_[index]);
    // return *object_ptr;
  }
  auto push_to_pool(T&& object) -> std::expected<size_t, Error> {
    T* object_ptr;
    std::unique_ptr<size_t> empty_slot(available_slots_.try_pop());
    if (empty_slot != nullptr) {
      *object_pool_[*empty_slot] = std::move(object);
      return *empty_slot;
    }
    object_ptr = new (std::nothrow) T(std::move(object));
    counter++;
    return validate_new(object_ptr);
  }

  auto push_to_pool_with_manager(T&& object) -> std::expected<size_t, Error> {
    T* object_ptr;
    std::unique_ptr<size_t> empty_slot(available_slots_.try_pop());
    if (empty_slot != nullptr) {
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
    available_slots_.push(available_slot);
  }
  size_t counter{0};

private:
  auto validate_new(T* object_ptr) -> std::expected<size_t, Error> {
    if (object_ptr != nullptr) {
      return push_back_impl(object_ptr);
    }
    return std::unexpected(Error{
        .backtrace = {"No more memory available, can't add object to pool"},
    });
  }
  auto push_back_impl(T* object_ptr) -> std::expected<size_t, Error> {
    try {
      object_pool_.push_back(object_ptr);
      return object_pool_.size() - 1;
    } catch (const std::exception& e) {
      LOG(e.what());
      return std::unexpected(
          Error{.backtrace = {"Unable to push new entry into pool"}});
    }
  }
  MemoryManager mmanager_;
  std::vector<T*> object_pool_;
  AtomicQueue<size_t> available_slots_;
};

} // namespace bsm

#endif
