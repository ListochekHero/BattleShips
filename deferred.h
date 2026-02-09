#ifndef DEFFERED_H
#define DEFFERED_H

#include <cstddef>
namespace bsm {
class Server;

struct DeferredAction {
  virtual ~DeferredAction() = default;
  virtual void run(Server& server) = 0;
};

struct CleanupSHP_Slot : DeferredAction { // Cleanup SocketHandler pool slot
  size_t slot;
  explicit CleanupSHP_Slot(size_t s) : slot(s) {}
  void run(Server& server) override;
};

} // namespace bsm
#endif
