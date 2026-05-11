#include "dispatcher.h"

#include "protocol/message_defs.h"
#include "protocol/message_types.h"
#include <algorithm>

namespace bsm {

auto Dispatcher::dispatch(const ReceivedMessage& message) -> ActionInfo {
  for (const auto& action : actions) {
    if (match_action(action, message)) {
      return {.scope = action.scope, .variant = action.make()};
    }
  }
  return {
      .scope = action_scope_e::NONE,
      .variant = Action::make_action<GeneralAction>(),
  };
}

auto Dispatcher::match_action(const Action& action,
                              const ReceivedMessage& message) -> bool {
  if (action.type && action.type == message.type) {
    return true;
  }
  return std::ranges::any_of(action.text_aliases,
                             [&message](auto alias) -> auto {
                               return message.payload.starts_with(alias);
                             });
}

const std::array<Dispatcher::Action, 8> Dispatcher::actions = {
    {
        {
            .text_aliases = {"\\create"},
            .type = std::nullopt,
            .scope = action_scope_e::BROADCAST,
            .make = Action::make_action<CreateLobby>,
        },
        {
            .text_aliases = {"\\close"},
            .type = std::nullopt,
            .scope = action_scope_e::NETWORK,
            .make = Action::make_action<Quit>,
        },
        {
            .text_aliases = {"\\quit"},
            .type = std::nullopt,
            .scope = action_scope_e::LOCAL,
            .make = Action::make_action<Quit>,
        },
        {
            .text_aliases = {"\\socket"},
            .type = message_type_e::SOCKET,
            .scope = action_scope_e::NETWORK,
            .make = Action::make_action<AcceptSocket>,
        },
        {
            .text_aliases = {"\\lobby_id"},
            .type = message_type_e::LOBBY_ID,
            .scope = action_scope_e::NETWORK,
            .make = Action::make_action<SetLobbyId>,
        },
        {
            .text_aliases = {"\\join"},
            .type = message_type_e::CONN_CODE,
            .scope = action_scope_e::BROADCAST,
            .make = Action::make_action<JoinLobby>,
        },
        {
            .text_aliases = {"\\msg"},
            .type = std::nullopt,
            .scope = action_scope_e::BROADCAST,
            .make = Action::make_action<ChatMessage>,
        },
        {
            .text_aliases = {"\\printable"},
            .type = message_type_e::PRINTABLE,
            .scope = action_scope_e::LOCAL,
            .make = Action::make_action<PrintMessage>,
        },
    },
};

} // namespace bsm
