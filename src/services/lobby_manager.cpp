#include "lobby_manager.h"

#include "utility/utility.h"

#include <cstdlib>
#include <signal.h>
#include <sys/socket.h>

namespace bsm {

std::expected<LobbyView, Error> LobbyManager::find(int64_t lobby_id) {
  auto it = lobbies_.find(lobby_id);
  if (it == lobbies_.end()) {
    return std::unexpected(Error{{"Lobby with given id doesn`t exist"}});
  }
  return LobbyView{lobby_id, it->second.control_connection};
}

std::expected<LobbyProcess, Error> LobbyManager::spawn_lobby() {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, sv) == -1)
    return std::unexpected(make_error_c("Failed to create socketpair: {}"));
  pid_t pid = fork();
  if (pid == -1)
    return std::unexpected(make_error_c("Failed to fork: {}"));
  if (pid == 0) {
    close(sv[1]);
    std::string socket_string{std::to_string(sv[0])};
    execl("/home/listochekhero/projects/battleships/build-debug/server",
          "./lobby", socket_string.c_str(), NULL);
    perror("execl failed");
    _exit(EXIT_FAILURE);
  }
  close(sv[0]);
  return LobbyProcess{pid, sv[1]};
}

std::expected<LobbyView, Error>
LobbyManager::attach(pid_t pid, ConnectionView control_connection) {
  int64_t lobby_id{generate_conn_code()};
  auto [it, inserted] =
      lobbies_.try_emplace(lobby_id, LobbyEntry{pid, control_connection});
  if (!inserted) {
    kill(pid, SIGKILL);
    return std::unexpected(Error{{"Unable to emplace lobby into map"}});
  }
  return LobbyView{lobby_id, control_connection};
}
} // namespace bsm
