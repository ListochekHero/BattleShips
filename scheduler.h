#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "atomic_queue.h"
#include "network_routine.h"
#include <any>
#include <array>
#include <atomic>
#include <coroutine>
#include <cstddef>
#include <map>
#include <memory>
#include <semaphore>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace bsm {

struct TaskContext {
  virtual ~TaskContext() = default;
};
struct NetworkTaskContext : TaskContext {
  NetworkTaskContext(size_t s) : slot(s) {};
  size_t slot;
};

struct Task {
  std::string task_tag{};
  std::unique_ptr<TaskContext> context{};
};

struct CoTask {
  std::thread co_executor{};
  std::optional<std::coroutine_handle<>> te_co_handle;
  bool running{false};
};

class Scheduler {
public:
  using TaskExecutor = std::function<void(std::unique_ptr<TaskContext>)>;

  void push_task(std::string task_tag, std::unique_ptr<TaskContext> context);
  Task* try_get_task();
  auto& get_executor_by_tag(std::string task_tag);
  void worker_loop();
  void run();
  bool add_executor(std::string tag, TaskExecutor executor) {
    auto [it, inserted] = tasks_executors_.try_emplace(tag, executor);
    return inserted;
  }
  template <typename F> size_t add_co_task(F&& f) {
    auto co_handle = std::invoke(f);
    co_tasks_.push_back(CoTask{.handle = co_handle});
    return co_tasks_.size() - 1;
  }
  template <typename F> void schedule_co_task(size_t slot, F&& f) {
    co_tasks_[slot].co_executor(f, *(co_tasks_[slot].te_co_handle));
    return;
  }

private:
  std::vector<CoTask> co_tasks_;
  AtomicQueue<Task> atomic_tasks_;
  std::vector<std::thread> thread_pool_{std::thread::hardware_concurrency()};
  std::counting_semaphore<std::numeric_limits<uint16_t>::max()> pop_c_semaphore_{
      0};
  std::counting_semaphore<std::numeric_limits<uint16_t>::max()>
      push_c_semaphore_{std::numeric_limits<uint16_t>::max() - 1};
  std::unordered_map<std::string, TaskExecutor> tasks_executors_;
};

} // namespace bsm
#endif
