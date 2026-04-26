#include "dispatcher.h"

#include "protocol/message_defs.h"
#include "protocol/message_types.h"
#include <algorithm>

namespace bsm {

auto Dispatcher::match_cmd(const Command& cmd, const ReadResult& msg) -> bool {
  if (cmd.msg_type && cmd.msg_type == msg.msg_type) {
    return true;
  }
  return std::ranges::any_of(cmd.text_aliases, [&msg](auto alias) -> auto {
    return msg.payload.starts_with(alias);
  });
}

auto Dispatcher::dispatch(const ReadResult& message) -> CommandInfo {
  for (const auto& cmd : commands) {
    if (match_cmd(cmd, message)) {
      return {.scope = cmd.scope, .parsed_cmd = cmd.make()};
    }
  }
  return {
      .scope = command_scope_e::NONE,
      .parsed_cmd = Command::make_action<GeneralAction>(),
  };
}

const std::array<Dispatcher::Command, 8> Dispatcher::commands = {
    {
        {
            .text_aliases = {"\\create"},
            .msg_type = std::nullopt,
            .scope = command_scope_e::BROADCAST,
            .make = Command::make_action<CreateLobby>,
        },
        {
            .text_aliases = {"\\close"},
            .msg_type = std::nullopt,
            .scope = command_scope_e::NETWORK,
            .make = Command::make_action<Quit>,
        },
        {
            .text_aliases = {"\\quit"},
            .msg_type = std::nullopt,
            .scope = command_scope_e::LOCAL,
            .make = Command::make_action<Quit>,
        },
        {
            .text_aliases = {"\\socket"},
            .msg_type = message_type_e::SOCKET,
            .scope = command_scope_e::NETWORK,
            .make = Command::make_action<AcceptSocket>,
        },
        {
            .text_aliases = {"\\lobby_id"},
            .msg_type = message_type_e::LOBBY_ID,
            .scope = command_scope_e::NETWORK,
            .make = Command::make_action<LobbyIdSetter>,
        },
        {
            .text_aliases = {"\\join"},
            .msg_type = message_type_e::CONN_CODE,
            .scope = command_scope_e::BROADCAST,
            .make = Command::make_action<JoinLobby>,
        },
        {
            .text_aliases = {"\\msg"},
            .msg_type = std::nullopt,
            .scope = command_scope_e::BROADCAST,
            .make = Command::make_action<ChatMessage>,
        },
        {
            .text_aliases = {"\\printable"},
            .msg_type = message_type_e::PRINTABLE,
            .scope = command_scope_e::LOCAL,
            .make = Command::make_action<PrintAble>,
        },
    },
};

} // namespace bsm
