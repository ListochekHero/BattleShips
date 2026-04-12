#ifndef MODULES_H
#define MODULES_H

#include "core/scheduler.h"

namespace bsm {

class Module {
public:
  virtual void attach_to_scheduler(Scheduler& scheduler) = 0;
  virtual ~Module() = default;
};

} // namespace bsm
#endif
