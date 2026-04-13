#ifndef MESSAGE_TYPES_H
#define MESSAGE_TYPES_H

#include "message_defs.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bsm {

struct MsgHeader {
  message_type_e msg_type{message_type_e::DEFAULT};
  uint64_t payload_count{0};
};

struct OutgoingMessage {
  std::vector<std::string_view> payloads;
  message_type_e msg_type{message_type_e::DEFAULT};
  std::optional<int> socket{std::nullopt};
};

struct ReadResult {
  message_status_e status{message_status_e::EMPTY};
  message_type_e msg_type{message_type_e::DEFAULT};
  std::string payload{std::string(1024, '\0')};
  std::optional<int> socket{std::nullopt};
};

} // namespace bsm

#endif
