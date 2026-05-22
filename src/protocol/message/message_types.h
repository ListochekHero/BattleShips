#ifndef MESSAGE_TYPES_H
#define MESSAGE_TYPES_H

#include "message_defs.h"
#include <cstdint>
#include <optional>
#include <string>

namespace bsm {

struct MessageHeader {
  message_type_e type{message_type_e::NONE};
  uint64_t payload_size{0};
};

struct OutgoingMessage {
  std::string_view payload;
  message_type_e type{message_type_e::DEFAULT};
  std::optional<int> socket{std::nullopt};
};

struct ReceivedMessage {
  message_status_e status{message_status_e::EMPTY};
  message_type_e type{message_type_e::DEFAULT};
  std::string payload;
  std::optional<int> socket{std::nullopt};
};

} // namespace bsm

#endif
