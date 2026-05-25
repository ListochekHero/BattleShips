#ifndef APP_LOBBY_H
#define APP_LOBBY_H

// IWYU pragma: begin_exports
#include "application/application.h"
#include "core/dispatcher.h"
#include "protocol/network/network_types.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
// IWYU pragma: end_export

namespace bsm {
enum class end_point_e : uint8_t;
class Lobby : public Application {
public:
  Lobby() = default;
  auto init(end_point_e socket_type, int parrent_socket)
      -> std::optional<Error>;
  void run() override;

private:
  using LobbyAction = std::variant<AcceptSocket, SetLobbyId, ChatMessage>;
  auto handle_action(const ActionContext& context) -> ActionResult override;
  auto execute_action(const AcceptSocket& action_type,
                      const ActionContext& context) -> ActionResult;
  auto send_join_lobby_message(ConnectionView recipient_view)
      -> std::optional<Error>;
  auto execute_action(const SetLobbyId& action_type,
                      const ActionContext& context) -> ActionResult;
  auto execute_action(const ChatMessage&, const ActionContext& context)
      -> ActionResult;
  int64_t lobby_id_{0};
  ConnectionView parent_view_{std::numeric_limits<std::size_t>::max()};
};

} // namespace bsm

#endif
