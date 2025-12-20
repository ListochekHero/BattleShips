#include "application.h"

namespace bsm {

Server::Server(int port) : epoll_handler(std::make_unique<EpollHandler>()) {
  auto server_socket = std::make_unique<SocketHandler>(port);
  epoll_handler->add_socket(server_socket->get_fd());
  server_socket->set_connection_callback(std::bind(
      &Server::handle_new_client_connection, this, std::placeholders::_1));
  attach(std::move(server_socket));
}
void Server::run() {
  while (true) {
    handle_zombie_pocesses();
    std::vector<int> ready_fds = epoll_handler->wait_for_events(MAX_EVENTS);
    for (int sock_fd : ready_fds) {
      notify(sock_fd);
    }
  }
}
void Server::attach(std::unique_ptr<Observer> observer) {
  SocketHandler *sock_handler = dynamic_cast<SocketHandler *>(observer.get());
  if (sock_handler) {
    observers[sock_handler->get_fd()] = std::move(observer);
  }
}
void Server::detach(std::unique_ptr<Observer> observer) {
  SocketHandler *sock_handler = dynamic_cast<SocketHandler *>(observer.get());
  if (sock_handler) {
    observers.erase(sock_handler->get_fd());
  }
}

void Server::notify(int sock_fd) {
  find_handler_by_socket(sock_fd)->update(sock_fd);
}

Observer *Server::find_handler_by_socket(int sock_fd) {
  auto it = observers.find(sock_fd);
  if (it != observers.end()) {
    return it->second.get();
  } else {
    return nullptr;
  }
}

void Server::handle_new_client_connection(int client_fd) {
  auto temp_handler = std::make_unique<SocketIOHandler>(client_fd);
  epoll_handler->add_socket(client_fd);
  temp_handler->set_connection_callback(std::bind(
      &Server::handle_client_request_for_lobby, this, std::placeholders::_1));
  attach(std::move(temp_handler));
}
void Server::handle_client_request_for_lobby(int client_fd) {
  LOG("handle_client_request_for_lobby");
  auto client_handler = std::move(
      dynamic_cast<SocketIOHandler *>(observers.find(client_fd)->second.get()));

  std::string client_request = client_handler->get_client_data();
  LOG(client_request);
  if (!client_request.compare("create")) {
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

      close_all_sockets_but_one(client_fd);

      execl("/home/listochekhero/projects/battleships/build/lobby", "lobby", std::to_string(client_handler->get_fd()).c_str(), nullptr);

      throw std::runtime_error("Failed to exec lobby");
    } else {
      close(pipe_fd[0]);
      close(client_fd);
    }
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
void Server::close_all_sockets_but_one(int sock_fd) {
  for (auto it = observers.begin(); it != observers.end();) {
    int current_fd = it->first;
    if (current_fd != sock_fd) {
      auto obserer = dynamic_cast<SocketHandler *>(it->second.get());
      it = observers.erase(it);
    } else {
      ++it;
    }
  }
}
}  // namespace BattleShipsMain
