#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "network_routine.h"
#include "utility.h"
#include <cstdint>
#include <variant>

namespace bsm {
struct CreateLobby {};
struct JoinLobby {
  int64_t lobby_id;
};
struct Quit {};
struct AcceptSocket {};
struct LobbyIdSetter {};
struct ChatMessage {};
struct NotAllowed {};
struct GeneralAction {};
using ParsedCommand =
    std::variant<CreateLobby, JoinLobby, Quit, AcceptSocket, LobbyIdSetter,
                 ChatMessage, NotAllowed, GeneralAction>;
enum class parsed_command_e {};
inline end_point_e operator|(end_point_e a, end_point_e b) {
  return (end_point_e)(uint8_t(a) | uint8_t(b));
}
inline bool allows(end_point_e m, end_point_e k) {
  return (uint8_t(m) & uint8_t(k)) != 0;
}
class Dispatcher {
public:
  ParsedCommand dispatch(const CommandContext& context);

private:
  struct Command {
    std::vector<std::string_view> text_aliases;
    std::optional<message_type_e> msg_type;
    end_point_e mask;

    using Factory = ParsedCommand (*)();

    template <typename T> static ParsedCommand make_action() { return T{}; }
    Factory make;
  };

  static const std::array<Command, 6> commands;
  bool match_cmd(const Command& cmd, const ReadResult& msg);
};

} // namespace bsm

#endif
