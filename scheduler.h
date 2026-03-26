#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "atomic_queue.h"
#include "network_routine.h"
#include <coroutine>
#include <cstddef>
#include <thread>
#include <vector>

namespace bsm {

struct Task {
  std::thread executor{};
  std::optional<std::coroutine_handle<>> co_handle;
  bool running{false};
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
          tasks.push_back({});
          tasks.back().executor(net_engine().process_client, *slot);
        }
      }
    }
  }

  template <typename F> size_t add_co_task(F&& f) {
    auto co_handle = std::invoke(f);
    tasks.push_back(Task{.handle = co_handle});
    return tasks.size() - 1;
  }
  template <typename F> void execute_co_task(size_t slot, F&& f) {
    tasks[slot].executor(f, *(tasks[slot].co_handle));
    return;
  }
  template <typename F> void execute_common_task(F&& f) {
    tasks.push_back(Task{});
    tasks.back().executor(f);
  }
  std::atomic_size_t pending_clients_counter_{0};
  AtomicQueue queue;

private:
  std::vector<Task> tasks;
};

} // namespace bsm
#endif
