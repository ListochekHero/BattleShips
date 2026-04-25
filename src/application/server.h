#ifndef SERVER_H
#define SERVER_H

#include "application/application.h"
#include "protocol/lobby_types.h"
#include "services/lobby_manager.h"

namespace bsm {

class Server : public Application {
public:
  Server() = default;
  void run() override;
  auto v_init(end_point_e socket_type, int parrent_socket) -> Ev;

private:
  static void handle_zombie_pocesses();

  using ServerAction =
      std::variant<CreateLobby, JoinLobby, ChatMessage, GeneralAction>;
  auto handle_client_cmd(const CommandContext& context)
      -> CommandStatus override;
  auto execute_action(const CreateLobby& action_type,
                      const CommandContext& context) -> CommandStatus;
  auto request_lobby() -> std::expected<LobbyView, Error>;
  auto execute_action(const JoinLobby& action_type,
                      const CommandContext& context) -> CommandStatus;
  auto execute_action(const ChatMessage& action_type,
                      const CommandContext& context) -> CommandStatus;
  auto execute_action(const GeneralAction& action_type,
                      const CommandContext& context) -> CommandStatus;

  LobbyManager lobby_manager_;
};

} // namespace bsm

#endif
