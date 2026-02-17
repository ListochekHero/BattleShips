#include "deferred_actions.h"
#include "application.h"

namespace bsm {

void CleanupSHP_Slot::run(NetworkEngine& nw_engine){
    nw_engine.cleanup_slot(slot);
}

} // namespace bsm
