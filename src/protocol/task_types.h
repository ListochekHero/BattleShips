#ifndef TASK_TYPES_H
#define TASK_TYPES_H

#include <cstddef>
#include <string>

namespace bsm {

struct TaskContext {
  virtual ~TaskContext() = default;
};
struct NetworkTaskContext : TaskContext {
  NetworkTaskContext(size_t s) : slot(s) {};
  size_t slot;
};

struct ConsoleTaskContext : TaskContext {
  ConsoleTaskContext(std::string&& s) : user_input(s) {};
  std::string user_input;
};

} // namespace bsm

#endif
