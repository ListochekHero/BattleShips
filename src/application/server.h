#ifndef SERVER_H
#define SERVER_H

#include "application/application.h"
#include "protocol/lobby_types.h"
#include "services/lobby_manager.h"

namespace bsm {

class Server : public Application {
public:
  Server() = default;
  Ev init();
  void run() override;
  Ev init(end_point_e socket_type, int parrent_socket);

private:
  void handle_zombie_pocesses();

  using ServerAction =
      std::variant<CreateLobby, JoinLobby, ChatMessage, GeneralAction>;
  CommandStatus handle_client_cmd(const CommandContext& context) override;
  CommandStatus execute_action(const CreateLobby& action_type,
                               const CommandContext& context);
  std::expected<LobbyView, Error> request_lobby();
  CommandStatus execute_action(const JoinLobby& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const ChatMessage& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const GeneralAction& action_type,
                               const CommandContext& context);

  LobbyManager lobby_manager_;
};

} // namespace bsm

#endif
