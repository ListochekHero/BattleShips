#include "application.h"

namespace bsm {

std::expected<void, std::string> Server::init() {
  this->epoll_handler.init();
  this->sockets.emplace_back();
  this->sockets.at(0).setup_listenter(Config::instance().get_line("port"));
}

void Server::run() {
  while (true) {
    handle_zombie_pocesses();
    auto result{epoll_handler.wait_for_events(MAX_EVENTS)};
    if (result) {
      for (SocketHandler* handler : *result) {
        if (handler->is_listening()) {
          auto new_clients{handler->accept_connections()};
          if (new_clients) {
            for (auto& client : *new_clients) {
              epoll_handler.add_socket(client);  // add error log
            }
            sockets.append_range(*new_clients | std::views::as_rvalue);
          } else {
            LOG(new_clients.error());
          }
        } else {
          auto result{handler->read_user_input()};
          if (result) {
            handle_client_cmd(*handler, *result);  // add error log
          }
        }
      }
    } else {
      LOG(result.error());
    }
  }
}

std::expected<void, std::string> Server::handle_client_cmd(
    const SocketHandler& client, std::string_view command) {
  using Handler =
      std::function<std::expected<void, std::string>(const SocketHandler&)>;
  LOG(command.data());
  inline static const std::unordered_map<std::string_view, Handler>
      command_map = {
          {"create",
           [this](const SocketHandler& c) { return create_lobby(c); }},
          {"test",
           [](const SocketHandler&) -> std::expected<void, std::string> {
             LOG("test");
             return {};
           }}};
  if (auto it = command_map.find(command); it != command_map.end()) {
    return it->second(client);
  } else
    return std::unexpected("Unknown command");
}

std::expected<void, std::string> Server::create_lobby(
    const SocketHandler& client) {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1)
    return std::unexpected("Failed to create pipe");
  inline static std::atomic_uint64_t next_uID{0};
  pid_t pid = fork();
  if (pid == -1) return std::unexpected("Failed to fork");
  if (pid == 0) {  // Child server process
    close(sv[1]);
    std::string socket_string{std::to_string(sv[0])};
    execl("/home/listochekhero/projects/battleships/build/lobby", "lobby",
          socket_string.c_str(), nullptr);
    perror("execl failed");
    _exit(127);
  } else {
    close(sv[0]);
    auto it = lobbies.emplace(next_uID++, sv[1]);
    return {};
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
      dynamic_cast<SocketIOHandler*>(observers.find(client_fd)->second.get()));

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

      execl("/home/listochekhero/projects/battleships/build/lobby", "lobby",
            std::to_string(client_handler->get_fd()).c_str(), nullptr);

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
      auto obserer = dynamic_cast<SocketHandler*>(it->second.get());
      it = observers.erase(it);
    } else {
      ++it;
    }
  }
}
}  // namespace bsm
