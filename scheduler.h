#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "atomic_queue.h"
#include "network_routine.h"
#include <any>
#include <coroutine>
#include <cstddef>
#include <thread>
#include <vector>

namespace bsm {

enum class task_status_e{EMPTY, READY, INPROGRESS};

struct Task {
  task_status_e task_status_v{task_status_e::EMPTY};
  std::any context{};
};

struct CoTask {
  std::thread executor{};
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
    co_tasks_[slot].executor(f, *(co_tasks_[slot].te_co_handle));
    return;
  }
  std::atomic_size_t pending_clients_counter_{0};

private:
  std::vector<CoTask> co_tasks_;
  std::vector<Task> tasks_;

};

} // namespace bsm
#endif
