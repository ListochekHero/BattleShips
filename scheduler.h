#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <coroutine>
#include <thread>
#include <vector>

namespace bsm {

class Scheduler {
public:
private:
  struct Task {
    std::thread executor;
    void* handle;
  };
  std::vector<Task> tasks;
};

} // namespace bsm
#endif
