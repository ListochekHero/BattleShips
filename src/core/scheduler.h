#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "atomic_queue.h"
#include "protocol/task_context_types.h"

#include <coroutine>
#include <cstddef>
#include <functional>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

namespace bsm {

enum class task_tag_e {NETWORK, CONSOLE};


struct Task {
  task_tag_e task_tag{};
  std::unique_ptr<TaskContext> context{};
};

struct CoTask {
  std::thread co_executor{};
  std::coroutine_handle<> te_co_handle;
  bool running{false};
};

class Scheduler {
public:
  void push_task(task_tag_e task_tag, std::unique_ptr<TaskContext> context);
  Task* try_get_task();
  using TaskExecutor = std::function<void(std::unique_ptr<TaskContext>)>;
  bool add_executor(task_tag_e tag, TaskExecutor executor);
  auto& get_executor_by_tag(task_tag_e task_tag);
  void worker_loop();
  void run();

  template <typename F> size_t add_co_task(F&& f) {
    auto co_handle = std::invoke(f);
    co_tasks_.push_back(CoTask{.te_co_handle = co_handle});
    return co_tasks_.size() - 1;
  }
  template <typename F> void schedule_co_task(size_t slot, F&& f) {
    co_tasks_[slot].co_executor = std::thread(f, co_tasks_[slot].te_co_handle);
    return;
  }

private:
  std::vector<CoTask> co_tasks_;
  AtomicQueue<Task> tasks_queue_;
  // std::vector<std::thread> thread_pool_{std::thread::hardware_concurrency() /
  //                                       2};
  std::vector<std::thread> thread_pool_{1};
  std::counting_semaphore<std::numeric_limits<uint16_t>::max()>
      pop_c_semaphore_{0};
  std::counting_semaphore<std::numeric_limits<uint16_t>::max()>
      push_c_semaphore_{std::numeric_limits<uint16_t>::max() - 1};
  std::unordered_map<task_tag_e, TaskExecutor> tasks_executors_;
};

} // namespace bsm
#endif
