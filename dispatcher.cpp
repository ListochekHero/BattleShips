#include "dispatcher.h"
#include "utility.h"

namespace bsm {

bool Server::match_cmd(const Command& cmd, const ReadResult& msg) {
  if (cmd.match.msg_type && cmd.match.msg_type == msg.msg_type)
    return true;
  for (auto alias : cmd.match.text_aliases)
    if (msg.payload.starts_with(alias))
      return true;
  return false;
}

ServerAction dispatch(const CommandContext& context) {

}

const std::array<Dispatcher::Command, 6> Dispatcher::commands = {
    {{.text_aliases = {"\\create"},
      .msg_type = std::nullopt,
      .make = [](const CommandContext&) -> std::optional<ServerAction> {
        return CreateLobby{};
      }},
     {.text_aliases = {"\\close"},
      .msg_type = std::nullopt,
      .make = [](const CommandContext&) -> std::optional<ServerAction> {
        return Quit{};
      }},
     {.text_aliases = {"\\socket"},
      .msg_type = message_type_e::SOCKET,
      .make = [](const CommandContext&) -> std::optional<ServerAction> {
        return AcceptSocket{};
      }},
     {.text_aliases = {"\\conn_code"},
      .msg_type = message_type_e::CONN_CODE,
      .make = [](const CommandContext&) -> std::optional<ServerAction> {
        return ConnectionCode{};
      }},
     {.text_aliases = {"\\join"},
      .msg_type = message_type_e::CONN_CODE,
      .make = [](const CommandContext&) -> std::optional<ServerAction> {
        return JoinLobby{};
      }},
     {.text_aliases = {"\\msg"},
      .msg_type = std::nullopt,
      .make = [](const CommandContext&) -> std::optional<ServerAction> {
        return ChatMessage{};
      }}}};

} // namespace bsm
