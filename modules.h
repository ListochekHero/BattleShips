#ifndef MODULES_H
#define MODULES_H

#include "dispatcher.h"
#include "network_routine.h"
#include "scheduler.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>

namespace bsm {

struct Module {
public:
  virtual void setup(Scheduler& scheduler) = 0;
  virtual ~Module() = default;
};

struct NetworkModule : Module {
  void setup(Scheduler& scheduler) override;

  NetworkEngine net_engine_;
};

} // namespace bsm
#endif
