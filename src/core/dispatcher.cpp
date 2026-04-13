#include "dispatcher.h"

#include "utility/utility.h"

namespace bsm {

bool Dispatcher::match_cmd(const Command& cmd, const ReadResult& msg) {
  if (cmd.msg_type && cmd.msg_type == msg.msg_type)
    return true;
  for (auto alias : cmd.text_aliases)
    if (msg.payload.starts_with(alias))
      return true;
  return false;
}

CommandInfo Dispatcher::dispatch(const ReadResult& message) {
  for (const auto& cmd : commands) {
    if (match_cmd(cmd, message))
      return {.scope = cmd.scope, .parsed_cmd = cmd.make()};
  }
  return {.scope = command_scope_e::NONE,
          .parsed_cmd = Command::make_action<GeneralAction>()};
}

const std::array<Dispatcher::Command, 8> Dispatcher::commands = {
    {{.text_aliases = {"\\create"},
      .msg_type = std::nullopt,
      .scope = command_scope_e::BROADCAST,
      .make = Command::make_action<CreateLobby>},
     {.text_aliases = {"\\close"},
      .msg_type = std::nullopt,
      .scope = command_scope_e::NETWORK,
      .make = Command::make_action<Quit>},
     {.text_aliases = {"\\quit"},
      .msg_type = std::nullopt,
      .scope = command_scope_e::LOCAL,
      .make = Command::make_action<Quit>},
     {.text_aliases = {"\\socket"},
      .msg_type = message_type_e::SOCKET,
      .scope = command_scope_e::NETWORK,
      .make = Command::make_action<AcceptSocket>},
     {.msg_type = message_type_e::LOBBY_ID,
      .scope = command_scope_e::NETWORK,
      .make = Command::make_action<LobbyIdSetter>},
     {.text_aliases = {"\\join"},
      .msg_type = message_type_e::CONN_CODE,
      .scope = command_scope_e::BROADCAST,
      .make = Command::make_action<JoinLobby>},
     {.text_aliases = {"\\msg"},
      .msg_type = std::nullopt,
      .scope = command_scope_e::BROADCAST,
      .make = Command::make_action<ChatMessage>},
     {.msg_type = message_type_e::PRINTABLE,
      .scope = command_scope_e::LOCAL,
      .make = Command::make_action<PrintAble>}}};

} // namespace bsm
