#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "atomic_queue.h"
#include "protocol/coroutine_promise.h"
#include "protocol/task_context_types.h"

#include <coroutine>
#include <cstddef>
#include <functional>
#include <memory>
#include <semaphore>
#include <thread>
#include <vector>

namespace bsm {

enum class task_tag_e { SCHEDULER, NETWORK, CONSOLE };

struct Producer {
  std::thread producer_thread;
};

struct Task {
  task_tag_e task_tag{};
  std::unique_ptr<TaskContext> context{};
};

class Scheduler {
public:
  void init();
  bool await_ready() { return false; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) {
    std::unique_ptr<task_tag_e> task_tag{available_task_tags_.try_pop()};
    return get_co_by_tag(*task_tag);
  };
  void await_resume() {}
  Scheduler(AtomicQueue<task_tag_e>& available_q)
      : available_task_tags_(available_q) {}
  void push_task(task_tag_e task_tag, std::unique_ptr<TaskContext> context);
  Task* try_get_task();
  using TaskExecutor = std::function<void(std::unique_ptr<TaskContext>)>;
  bool add_executor(task_tag_e tag, TaskExecutor executor);
  auto& get_executor_by_tag(task_tag_e task_tag);
  std::coroutine_handle<> get_co_by_tag(task_tag_e task_tag);
  void worker_loop();
  void run_workers();
  bsm_co_handle co_run();
  template <typename F> void add_and_run_producer(F&& f) {
    producers_.emplace_back(std::thread(f));
  }
  template <typename F> void add_co_task(F&& f, task_tag_e task_tag) {
    auto co_handle = std::invoke(f);
    co_handlers_map_.try_emplace(task_tag, co_handle);
  }

private:
  std::vector<Producer> producers_;
  std::unordered_map<task_tag_e, std::coroutine_handle<>>
      co_handlers_map_;
  AtomicQueue<Task> tasks_queue_;
  AtomicQueue<task_tag_e>& available_task_tags_;

  // std::vector<std::thread> thread_pool_{std::thread::hardware_concurrency() /
  //                                       2};
  std::vector<std::thread> thread_pool_{1};
  std::counting_semaphore<std::numeric_limits<uint16_t>::max()>
      pop_c_semaphore_{0};
  std::counting_semaphore<std::numeric_limits<uint16_t>::max()>
      push_c_semaphore_{std::numeric_limits<uint16_t>::max() - 1};
  std::unordered_map<task_tag_e, TaskExecutor> task_executors_;
};

} // namespace bsm

#endif
