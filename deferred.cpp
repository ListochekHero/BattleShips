#include "deferred.h"
#include "application.h"
namespace bsm {

void CleanupSHP_Slot::run(Server& server){
    server.cleanup_slot(slot);
}

} // namespace bsm
