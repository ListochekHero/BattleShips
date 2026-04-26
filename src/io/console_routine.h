#ifndef CONSOLE_ROUTINE_H
#define CONSOLE_ROUTINE_H

#include "interfaces/modules.h"

#include <cstdint>
#include <string>

namespace bsm {

enum class task_tag_e : uint8_t;
template <typename T> class AtomicQueue;

class ConsoleHandler : public Module {
public:
  ConsoleHandler(AtomicQueue<std::string>& console_q_,
                 AtomicQueue<task_tag_e>& available_q_)
      : console_raw_tasks_(console_q_), available_task_tags_(available_q_) {};
  void run();

private:
  AtomicQueue<std::string>& console_raw_tasks_;
  AtomicQueue<task_tag_e>& available_task_tags_;
};

} // namespace bsm

#endif
