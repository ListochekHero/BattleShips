#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "atomic_queue.h"
#include "network_routine.h"
#include <any>
#include <atomic>
#include <coroutine>
#include <cstddef>
#include <map>
#include <memory>
#include <semaphore>
#include <thread>
#include <vector>

namespace bsm {

struct TaskContext {
  virtual ~TaskContext() = default;
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

class ThreadPool {
  std::vector<std::thread> thread_pool_{4};
};

class Scheduler {
public:
  void run() {
    size_t counter{0};
    while (!counter) {
      pending_clients_counter_.wait(0);
      counter = pending_clients_counter_.load();
      while (counter) {
        auto slot{queue.single_thread_pop()};
        if (slot) {
          co_tasks_.push_back({});
          co_tasks_.back().executor(net_engine().process_client, *slot);
        }
      }
    }
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
  std::atomic_size_t pending_clients_counter_{0};

  auto& get_executor_by_tag(std::string task_tag) {
    return tasks_executors[task_tag];
  }
  void push_task(std::string task_tag, std::unique_ptr<TaskContext> context) {
    Task* task = new Task(task_tag, std::move(context));
    atomic_tasks_.push(task);
    c_semaphore_.release();
  }

  Task* try_get_task() {
    c_semaphore_.acquire();
    Task* task = atomic_tasks_.pop();
      return task;
  }

private:
  std::vector<CoTask> co_tasks_;
  AtomicQueue<Task> atomic_tasks_;
  std::counting_semaphore<std::numeric_limits<uint8_t>::max()> c_semaphore_;
  using TaskExecutor = void (*)(std::unique_ptr<TaskContext> task_context);
  std::map<std::string, TaskExecutor> tasks_executors;
};

} // namespace bsm
#endif
