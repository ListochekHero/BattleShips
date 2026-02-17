#ifndef DEFFERED_ACTIONS_H
#define DEFFERED_ACTIONS_H

#include "network_routine.h"
#include <cstddef>
namespace bsm {

class NetworkEngine;
class ConnectionView;

struct DeferredAction {
  virtual ~DeferredAction() = default;
  virtual void run(NetworkEngine& nw_engine) = 0;
};

struct CleanupSHP_Slot : DeferredAction { // Cleanup SocketHandler pool slot
  size_t slot;
  explicit CleanupSHP_Slot(size_t s) : slot(s) {}
  explicit CleanupSHP_Slot(ConnectionView view) : slot(view.get_slot()) {}
  void run(NetworkEngine& nw_engine) override;
};

} // namespace bsm
#endif
