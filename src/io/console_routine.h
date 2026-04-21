#ifndef CONSOLE_ROUTINE_H
#define CONSOLE_ROUTINE_H

#include "core/scheduler.h"
#include "interfaces/modules.h"

#include <string>

namespace bsm {

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
