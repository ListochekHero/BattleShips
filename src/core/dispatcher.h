#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "net/socket_routine.h"
#include "utility/utility.h"
#include <variant>

namespace bsm {
struct CreateLobby {};
struct JoinLobby {};
struct Quit {};
struct AcceptSocket {};
struct LobbyIdSetter {};
struct ChatMessage {};
struct PrintAble {};
struct NotAllowed {};
struct GeneralAction {};
using ParsedCommand =
    std::variant<CreateLobby, JoinLobby, Quit, AcceptSocket, LobbyIdSetter,
                 ChatMessage, PrintAble, NotAllowed, GeneralAction>;
enum class command_scope_e { NONE, LOCAL, BROADCAST, NETWORK };
struct CommandInfo {
  command_scope_e scope{command_scope_e::NONE};
  ParsedCommand parsed_cmd;
};

class Dispatcher {
public:
  CommandInfo dispatch(const ReadResult& message);

private:
  struct Command {
    std::vector<std::string_view> text_aliases;
    std::optional<message_type_e> msg_type;
    command_scope_e scope;

    using Factory = ParsedCommand (*)();

    template <typename T> static ParsedCommand make_action() { return T{}; }
    Factory make;
  };

  static const std::array<Command, 8> commands;
  bool match_cmd(const Command& cmd, const ReadResult& msg);
};

} // namespace bsm

#endif
