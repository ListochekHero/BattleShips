#include "memory_manager.h"

#include "utility/error.h"

#include <atomic>
#include <cstdint>
#include <fcntl.h>
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
  auto& meta_struct{get_meta_data()};
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

auto MemoryManager::allocate_new_node() -> std::optional<Error> {
  auto& meta_struct{get_meta_data()};
  int64_t memory_amount{meta_struct.page_size * NODE_SIZE};
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
  meta_struct.actual_pages_allocated += (NODE_SIZE);
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
