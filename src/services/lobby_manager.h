#ifndef LOBBY_MANAGER_H
#define LOBBY_MANAGER_H

#include "protocol/lobby_types.h"
#include "protocol/network_types.h"
#include "utility/error.h"

#include <cstdint>
#include <expected>
#include <sys/types.h>
#include <unordered_map>

namespace bsm {

struct LobbyProcess {
  pid_t pid;
  int control_socket;
};

struct LobbyEntry {
  pid_t pid;
  ConnectionView control_connection;
};

class LobbyManager {
public:
  auto find(int64_t lobby_id) -> std::expected<LobbyView, Error>;
  static auto spawn_lobby() -> std::expected<LobbyProcess, Error>;
  auto attach(pid_t pid, ConnectionView control_connection)
      -> std::expected<LobbyView, Error>;

private:
  std::unordered_map<int64_t, LobbyEntry> lobbies_;
};

} // namespace bsm

#endif
