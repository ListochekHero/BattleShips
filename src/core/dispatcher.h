#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace bsm {

enum class message_type_e : uint8_t;
struct ReceiveResult;

struct CreateLobby {};
struct JoinLobby {};
struct Quit {};
struct AcceptSocket {};
struct SetLobbyId {};
struct ChatMessage {};
struct PrintMessage {};
struct NotAllowed {};
struct GeneralAction {};
using ActionVariant =
    std::variant<CreateLobby, JoinLobby, Quit, AcceptSocket, SetLobbyId,
                 ChatMessage, PrintMessage, NotAllowed, GeneralAction>;

enum class action_scope_e : uint8_t { NONE, LOCAL, BROADCAST, NETWORK };

struct ActionInfo {
  action_scope_e scope{action_scope_e::NONE};
  ActionVariant variant;
};

class Dispatcher {
public:
  static auto dispatch(const ReceiveResult& message) -> ActionInfo;

private:
  struct Action {
    std::vector<std::string_view> text_aliases;
    std::optional<message_type_e> type;
    action_scope_e scope;

    using Factory = ActionVariant (*)();

    template <typename T> static auto make_action() -> ActionVariant {
      return T{};
    }
    Factory make;
  };

  static const std::array<Action, 8> actions;
  static auto match_action(const Action& action,
                            const ReceiveResult& message) -> bool;
};

} // namespace bsm

#endif
