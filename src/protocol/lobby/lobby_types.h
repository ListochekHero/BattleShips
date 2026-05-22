#ifndef LOBBY_TYPES_H
#define LOBBY_TYPES_H

#include "network_types.h"
#include <cstdint>

namespace bsm {

struct LobbyView {
  int64_t lobby_id;
  ConnectionView control_connection;
};

} // namespace bsm

#endif
