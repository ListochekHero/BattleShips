#include "server.h"

namespace BattleShipsMain {

Server::Server(int port)
    : server_socket(new SocketHandler(port)),
      epoll_handler(new EpollHandler()) {
  epoll_handler.get()->add_socket(server_socket.get()->get_fd());
  server_socket.get()->set_connection_callback(
      std::bind(&Server::handle_connection, this, std::placeholders::_1));
}
void Server::run() {
  while (true) {
    handle_zombie_pocesses();
    epoll_handler.get()->wait_for_events(MAX_EVENTS);
  }
}

void Server::handle_connection(int client_fd) {
  int pipe_fd[2];
  if (pipe(pipe_fd) == -1) {
    throw std::runtime_error("Failed to create pipe");
  }

  pid_t pid = fork();
  if (pid == -1) {
    throw std::runtime_error("Failed to fork");
  }

  if (pid == 0) {  // Child server process
    close(pipe_fd[1]);
    dup2(pipe_fd[0], STDIN_FILENO);
    close(pipe_fd[0]);

    close(server_socket.get()->get_fd());

    execl("./", "lobby", std::to_string(client_fd).c_str(), nullptr);

    throw std::runtime_error("Failed to exec lobby");
  } else {
    close(pipe_fd[0]);
    close(client_fd);
  }
}

void Server::handle_zombie_pocesses() {
  int status;
  pid_t pid;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (WIFEXITED(status)) {
      // TODO logging
    }
  }
}
}  // namespace BattleShipsMain