// IWYU pragma: no_include <vector>
#include "lobby_manager.h"

#include "utility/utility.h"

#include <csignal> // IWYU pragma: keep
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <utility> // IWYU pragma: keep

namespace bsm {

auto LobbyManager::find(int64_t lobby_id) -> std::expected<LobbyView, Error> {
  auto iter = lobbies_.find(lobby_id);
  if (iter == lobbies_.end()) {
    return std::unexpected(
        Error{.backtrace = {"Lobby with given id doesn`t exist"}});
  }
  return LobbyView{.lobby_id = lobby_id,
                   .control_connection = iter->second.control_connection};
}

auto LobbyManager::spawn_lobby() -> std::expected<LobbyProcess, Error> {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, sv) == -1) {
    return std::unexpected(make_error_c("Failed to create socketpair: {}"));
  }
  pid_t pid = fork();
  if (pid == -1) {
    return std::unexpected(make_error_c("Failed to fork: {}"));
  }
  if (pid == 0) {
    close(sv[1]);
    std::string socket_string{std::to_string(sv[0])};
    execl("/home/listochekhero/projects/battleships/build-debug/server",
          "./lobby", socket_string.c_str(), NULL);
    perror("execl failed");
    _exit(EXIT_FAILURE);
  }
  close(sv[0]);
  return LobbyProcess{.pid = pid, .control_socket = sv[1]};
}

auto LobbyManager::attach(pid_t pid, ConnectionView control_connection)
    -> std::expected<LobbyView, Error> {
  int64_t lobby_id{generate_conn_code()};
  auto [it, inserted] = lobbies_.try_emplace(
      lobby_id,
      LobbyEntry{.pid = pid, .control_connection = control_connection});
  if (!inserted) {
    kill(pid, SIGKILL);
    return std::unexpected(
        Error{.backtrace = {"Unable to emplace lobby into map"}});
  }
  return LobbyView{
      .lobby_id = lobby_id,
      .control_connection = control_connection,
  };
}

void LobbyManager::kill_lobby(int64_t lobby_id) {
  kill(lobbies_[lobby_id].pid, SIGKILL);
}

} // namespace bsm
