#ifndef LOBBY_MANAGER_H
#define LOBBY_MANAGER_H

#include "net/network_routine.h"
#include "utility/utility.h"
#include <cstdint>
#include <expected>
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

struct LobbyView {
  int64_t lobby_id;
  ConnectionView control_connection;
};

class LobbyManager {
public:
  std::expected<LobbyView, Error> find(int64_t lobby_id);
  std::expected<LobbyProcess, Error> spawn_lobby();
  std::expected<LobbyView, Error> attach(pid_t pid,
                                         ConnectionView control_connection);

private:
  std::unordered_map<int64_t, LobbyEntry> lobbies_;
};

} // namespace bsm

#endif
