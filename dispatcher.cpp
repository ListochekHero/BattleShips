#include "dispatcher.h"
#include "network_routine.h"
#include "utility.h"

namespace bsm {

bool Dispatcher::match_cmd(const Command& cmd, const ReadResult& msg) {
  if (cmd.msg_type && cmd.msg_type == msg.msg_type)
    return true;
  for (auto alias : cmd.text_aliases)
    if (msg.payload.starts_with(alias))
      return true;
  return false;
}

ParsedCommand Dispatcher::dispatch(const CommandContext& context) {
  for (const auto& cmd : commands) {
    if (match_cmd(cmd, context.message)) {
      if (allows(cmd.mask, context.peer)) {
        return cmd.make();
      } else {
        return Command::make_action<NotAllowed>();
      }
    }
  }
  return Command::make_action<GeneralAction>();
}

const std::array<Dispatcher::Command, 6> Dispatcher::commands = {
    {{.text_aliases = {"\\create"},
      .msg_type = std::nullopt,
      .mask = end_point_e::CLIENT,
      .make = Command::make_action<CreateLobby>},
     {.text_aliases = {"\\close"},
      .msg_type = std::nullopt,
      .mask = end_point_e::CLIENT,
      .make = Command::make_action<Quit>},
     {.text_aliases = {"\\socket"},
      .msg_type = message_type_e::SOCKET,
      .mask = end_point_e::SERVER | end_point_e::PARENT,
      .make = Command::make_action<AcceptSocket>},
     {.msg_type = message_type_e::LOBBY_ID,
      .mask = end_point_e::PARENT,
      .make = Command::make_action<LobbyIdSetter>},
     {.text_aliases = {"\\join"},
      .msg_type = message_type_e::CONN_CODE,
      .mask = end_point_e::CLIENT,
      .make = Command::make_action<JoinLobby>},
     {.text_aliases = {"\\msg"},
      .msg_type = std::nullopt,
      .mask = end_point_e::CLIENT,
      .make = Command::make_action<ChatMessage>}}};

} // namespace bsm
