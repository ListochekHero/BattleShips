#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace bsm {

enum class message_type_e : uint8_t;
struct ReadResult;

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

enum class command_scope_e : uint8_t { NONE, LOCAL, BROADCAST, NETWORK };

struct CommandInfo {
  command_scope_e scope{command_scope_e::NONE};
  ParsedCommand parsed_cmd;
};

class Dispatcher {
public:
  static auto dispatch(const ReadResult& message) -> CommandInfo;

private:
  struct Command {
    std::vector<std::string_view> text_aliases;
    std::optional<message_type_e> msg_type;
    command_scope_e scope;

    using Factory = ParsedCommand (*)();

    template <typename T> static auto make_action() -> ParsedCommand {
      return T{};
    }
    Factory make;
  };

  static const std::array<Command, 8> commands;
  static auto match_cmd(const Command& cmd, const ReadResult& msg) -> bool;
};

} // namespace bsm

#endif
