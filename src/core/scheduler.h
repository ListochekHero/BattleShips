#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "atomic_queue.h"
#include "protocol/coroutine/coroutine_promise.h"
#include "protocol/task/task_context_types.h"
#include "utility/error.h"

// IWYU pragma: no_include <string>
#include <coroutine>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <semaphore>
#include <thread>
#include <unordered_map>
#include <vector>

namespace bsm {

enum class task_tag_e : uint8_t { SCHEDULER, NETWORK, CONSOLE };

struct Producer {
  std::thread producer_thread;
};

struct Task {
  task_tag_e tag{};
  std::unique_ptr<TaskContext> context;
};

class Scheduler {
public:
  Scheduler(AtomicQueue<task_tag_e>& available_q)
      : available_task_tags_(available_q) {}
  void push_task(task_tag_e tag, std::unique_ptr<TaskContext> context);
  using TaskExecutor = std::function<void(std::unique_ptr<TaskContext>)>;
  auto add_task_executor(task_tag_e tag, TaskExecutor executor)
      -> std::optional<Error>;
  template <typename Pf> void add_and_run_producer(Pf&& producer_func) {
    producers_.emplace_back(std::thread(producer_func));
  }
  void run_workers();
  auto get_co_handle_by_tag(task_tag_e task_tag) -> std::coroutine_handle<>;
  template <typename Cf>
  auto add_co_handle(Cf&& coroutine_func, task_tag_e tag)
      -> std::optional<Error> {
    auto coroutine_handle = std::invoke(coroutine_func);
    auto [itter, inserted] = co_handles_map_.try_emplace(tag, coroutine_handle);
    if (!inserted) {
      return Error{
          .backtrace =
              {"Unable to add coroutine handle: failed to emplace into map"},
      };
    }
    return std::nullopt;
  }

  auto coroutine_loop() -> bsm_co_handle;

private:
  auto try_get_task() -> Task*;
  auto get_executor_by_tag(task_tag_e tag) -> auto&;
  void worker_loop();
  auto yield_to_application() { return SchedulerAwaiter(*this); }

  std::vector<Producer> producers_;
  std::unordered_map<task_tag_e, std::coroutine_handle<>> co_handles_map_;
  AtomicQueue<task_tag_e>& available_task_tags_;
  AtomicQueue<Task> tasks_queue_;
  std::counting_semaphore<std::numeric_limits<uint16_t>::max()>
      pop_task_semaphore_{0};
  std::counting_semaphore<std::numeric_limits<
      uint16_t>::max()> // make define for this number to use in AtomicQueue as
                        // well for consistency
      push_task_semaphore_{std::numeric_limits<uint16_t>::max() - 1};
  std::unordered_map<task_tag_e, TaskExecutor> task_executors_;

  // std::vector<std::thread> thread_pool_{std::thread::hardware_concurrency() /
  //                                       2};
  std::vector<std::thread> thread_pool_{1};

  struct SchedulerAwaiter {
    Scheduler& scheduler;
    static auto await_ready() -> bool { return false; }
    auto await_suspend(std::coroutine_handle<> /*unused*/)
        -> std::coroutine_handle<> {
      std::unique_ptr<task_tag_e> task_tag{
          scheduler.available_task_tags_.try_pop()};
      return scheduler.get_co_handle_by_tag(*task_tag);
    };
    void await_resume() {}
  };
};

} // namespace bsm

#endif
