#ifndef TASK_CONTEXT_TYPES_H
#define TASK_CONTEXT_TYPES_H

#include <cstddef>
#include <string>

namespace bsm {

struct TaskContext {
  virtual ~TaskContext() = default;
};
struct NetworkTaskContext : TaskContext {
  NetworkTaskContext(size_t slot) : slot_(slot) {};
  size_t slot_;
};

struct ConsoleTaskContext : TaskContext {
  ConsoleTaskContext(std::string&& input) : input_(input) {};
  std::string input_;
};

} // namespace bsm

#endif
