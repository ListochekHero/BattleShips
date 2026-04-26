#ifndef APP_LOBBY_H
#define APP_LOBBY_H

// IWYU pragma: begin_exports
#include "application/application.h"
#include "core/dispatcher.h"
#include "protocol/network_types.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <variant>
// IWYU pragma: end_export

namespace bsm {
enum class end_point_e : uint8_t;
class Lobby : public Application {
public:
  Lobby() = default;
  auto v_init(end_point_e socket_type, int parrent_socket) -> Ev;
  void run() override;

private:
  using LobbyAction = std::variant<AcceptSocket, LobbyIdSetter, ChatMessage>;
  auto handle_client_cmd(const CommandContext& context)
      -> CommandStatus override;
  auto execute_action(const AcceptSocket& action_type,
                      const CommandContext& context) -> CommandStatus;
  auto execute_action(const LobbyIdSetter& action_type,
                      const CommandContext& context) -> CommandStatus;
  auto execute_action(const ChatMessage&, const CommandContext& context)
      -> CommandStatus;
  int64_t lobby_id_{0};
  ConnectionView parent_view_{std::numeric_limits<std::size_t>::max()};
};

} // namespace bsm

#endif
