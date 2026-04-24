#include "deferred_actions.h"

#include <utility>

namespace bsm {

void DeferredActions::schedule(std::unique_ptr<DeferredAction> action,
                               NetworkEngine& engine) {
  action->prepare(engine);
  actions_.push_back(std::move(action));
}
void DeferredActions::flush(NetworkEngine& engine) {
  for (auto& action : actions_) {
    action->execute(engine);
  }
  actions_.clear();
}

} // namespace bsm
