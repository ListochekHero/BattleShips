#ifndef SERVER_H
#define SERVER_H

// IWYU pragma: begin_exports
#include "application/application.h"
#include "protocol/lobby_types.h"
#include "services/lobby_manager.h"
#include "utility/utility.h"
// IWYU pragma: end_export

namespace bsm {

class Server : public Application {
public:
  Server() = default;
  void run() override;
  auto init(end_point_e socket_type, int parrent_socket)
      -> std::optional<Error>;

private:
  static void handle_zombie_pocesses();

  using ServerAction =
      std::variant<CreateLobby, JoinLobby, ChatMessage, GeneralAction>;
  auto handle_action(const ActionContext& context) -> ActionResult override;
  auto execute_action(const CreateLobby& action_type,
                      const ActionContext& context) -> ActionResult;
  auto create_lobby() -> std::expected<LobbyView, Error>;
  auto init_lobby(const LobbyView& lobby_view) -> std::optional<Error>;
  auto perform_transfer(const LobbyView& recipient, ActionContext context)
      -> ActionResult;
  auto execute_action(const JoinLobby& action_type,
                      const ActionContext& context) -> ActionResult;
  auto execute_action(const ChatMessage& action_type,
                      const ActionContext& context) -> ActionResult;
  auto execute_action(const GeneralAction& action_type,
                      const ActionContext& context) -> ActionResult;

  LobbyManager lobby_manager_;
};

} // namespace bsm

#endif
