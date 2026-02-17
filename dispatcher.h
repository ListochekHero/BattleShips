#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "application.h"
#include "data_storage.h"
#include "network_routine.h"
#include "utility.h"
#include <cstdint>
#include <expected>
#include <unordered_map>
#include <variant>

namespace bsm {
struct CreateLobby {};
struct JoinLobby {
  int64_t lobby_id;
};
struct Quit {};
struct AcceptSocket {};
struct ConnectionCode {};
struct ChatMessage {};

using ServerAction = std::variant<CreateLobby, JoinLobby, Quit, AcceptSocket,
                                  ConnectionCode, ChatMessage>;

enum class PeerMask : uint8_t { None = 0, CLIENT = 1 << 0, LOBBY = 1 << 1 };
inline PeerMask operator| (PeerMask a, PeerMask b){
  return PeerMask(uint8_t(a)| uint8_t(b));
}
inline bool allows(PeerMask m, PeerKind k){
  return (uint8_t(m) & uint8_t(k));
}
class Dispatcher {
public:
  ServerAction dispatch(const CommandContext& context);

private:
  struct Command {
    std::vector<std::string_view> text_aliases;
    std::optional<message_type_e> msg_type;
    using Factory = std::optional<ServerAction> (*)(const CommandContext&);
    Factory make;
  };
  static const std::array<Command, 6> commands;
  bool match_cmd(const Command& cmd, const ReadResult& msg);
};

} // namespace bsm

#endif
