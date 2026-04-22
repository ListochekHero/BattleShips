#ifndef APP_LOBBY_H
#define APP_LOBBY_H

#include "application/application.h"

namespace bsm {

class Lobby : public Application {
public:
  Lobby() = default;
  Ev init(end_point_e socket_type, int parrent_socket);
  void run() override;

private:
  using LobbyAction = std::variant<AcceptSocket, LobbyIdSetter, ChatMessage>;
  CommandStatus handle_client_cmd(const CommandContext& context) override;
  CommandStatus execute_action(const AcceptSocket& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const LobbyIdSetter& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const ChatMessage&,
                               const CommandContext& context);
  int64_t lobby_id_{0};
  ConnectionView parent_view_{std::numeric_limits<std::size_t>::max()};
};

} // namespace bsm

#endif
