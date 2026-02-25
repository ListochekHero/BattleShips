#ifndef DEFFERED_ACTIONS_H
#define DEFFERED_ACTIONS_H

#include <memory>
#include <vector>

namespace bsm {

class NetworkEngine;
class ConnectionView;

struct DeferredAction {
  virtual ~DeferredAction() = default;
  virtual void prepare(NetworkEngine& engine) = 0;
  virtual void execute(NetworkEngine& engine) = 0;
};

class DeferredActions {
public:
  void schedule(std::unique_ptr<DeferredAction> action, NetworkEngine& engine);
  void flush(NetworkEngine& engine);

private:
  std::vector<std::unique_ptr<DeferredAction>> actions_;
};

} // namespace bsm
#endif
