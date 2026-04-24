#ifndef MODULES_H
#define MODULES_H

namespace bsm {

class Scheduler;

class Module {
public:
  virtual void attach_to_scheduler(Scheduler& /*unused*/) {};
  virtual ~Module() = default;
};

} // namespace bsm
#endif
