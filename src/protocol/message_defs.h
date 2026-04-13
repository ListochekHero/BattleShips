#ifndef MESSAGE_DEFS_H
#define MESSAGE_DEFS_H
#include <cstdint>

namespace bsm {

enum class message_type_e : uint8_t {
  DEFAULT,
  PRINTABLE,
  SOCKET,
  CONN_CODE,
  LOBBY_ID,
  ERROR
};

enum class message_status_e { EMPTY, WOULDBLOCK, DISCONNECTED, DATA };

} // namespace bsm

#endif
