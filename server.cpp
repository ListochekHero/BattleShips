#include "server.h"

namespace BattleShipsMain {

Server::Server(int port) : server_socket(port), epoll_handler() {
  epoll_handler.add_socket(server_socket.get_fd());
}
void Server::run() {
  while (true) {
    handle_zombie_pocesses();
    auto ready_fds = epoll_handler.wait_for_events(MAX_EVENTS);
    for (int sock_fd : ready_fds) {
      if (sock_fd == server_socket.get_fd()) {
        this->handle_connection(server_socket.accept_connection());
      }
    }
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

    close(server_socket.get_fd());

    execl("./", "lobby", std::to_string(client_fd).c_str(), nullptr);

    throw std::runtime_error("Failed to exec lobby");
  } else {
    close(pipe_fd[0]);
    close(client_fd);
  }
}

void Server::handle_zombie_pocesses(){
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFEXITED(status))
        {
            //todo logging
        }
        
    }
    
}
}  // namespace BattleShipsMain