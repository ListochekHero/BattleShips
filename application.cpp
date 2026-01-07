#include "application.h"

namespace bsm {

std::expected<void, std::string> Server::init() {
  LOG("Server::init()");
  this->epoll_handler.init();
  this->sockets.emplace_back(std::make_unique<SocketHandler>());
  this->sockets[0].get()->setup_listenter(Config::instance().get_line("port"));
  this->epoll_handler.add_socket(sockets[0].get());
  return {};
}

void Server::run() {
  LOG("Server::run()");
  while (true) {
    handle_zombie_pocesses();
    auto result{epoll_handler.wait_for_events(MAX_EVENTS)};
    if (result) {
      for (SocketHandler* handler : *result) {
        if (handler->socket_type() == socket_type_e::SERVER) {
          auto new_clients{handler->accept_connections()};
          if (new_clients) {
            for (auto& client : *new_clients) {
              epoll_handler.add_socket(client.get());  // add error log
            }
            sockets.append_range(*new_clients | std::views::as_rvalue);
          } else {
            LOG(new_clients.error());
          }
        } else {
          auto result{handler->read_user_input()};
          if (result) {
            handle_client_cmd(*handler, *result);  // add error log
          } else {
          }  // add SocketHandler deletion on "Connection closed"
        }
      }
    } else {
      LOG(result.error());
    }
  }
}

std::expected<void, std::string> Server::handle_client_cmd(
    const SocketHandler& client, std::string_view command) {
  LOG("Server::handle_client_cmd()");
  using Handler =
      std::function<std::expected<void, std::string>(const SocketHandler&)>;
  LOG(command.data());
  static const std::unordered_map<std::string_view, Handler> command_map = {
      {"create", [this](const SocketHandler& c) { return create_lobby(c); }},
      {"close",
       [this](const SocketHandler& c) { return erase_socket_handler(c); }},
      {"test", [](const SocketHandler&) -> std::expected<void, std::string> {
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
  LOG("Server::create_lobby()");
  epoll_handler.remove_socket(std::addressof(client));
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, sv) == -1)
    return std::unexpected("Failed to create socketpair");
  pid_t pid = fork();
  if (pid == -1) return std::unexpected("Failed to fork");
  if (pid == 0) {  // Child server process
    close(sv[1]);
    std::string socket_string{std::to_string(sv[0])};
    execl("/home/listochekhero/projects/battleships/build/lobby", "./lobby",
          socket_string.c_str(), NULL);
    perror("execl failed");
    _exit(127);
  } else {
    close(sv[0]);
    std::time_t now = std::time(nullptr);
    LOG(std::format("\"{}\" - time", now));
    auto it = lobbies.emplace(now, sv[1]);
    it.first->second.write_to_user("socket");
    it.first->second.write_to_user(std::to_string(client.get_socket()));
    erase_socket_handler(client);
    it.first->second.write_to_user("conn_code");
    it.first->second.write_to_user(std::to_string(it.first->first);
    return {};
  }
}
std::expected<void, std::string> Server::erase_socket_handler(
    const SocketHandler& client) {
  std::erase_if(sockets, [&client](const auto& s) {
    return s.get()->get_socket() == client.get_socket();
  });
  return {};
}

void Server::handle_zombie_pocesses() {
  LOG("Server::handle_zombie_pocesses()");
  int status;
  pid_t pid;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (WIFEXITED(status)) {
      // TODO logging
    }
  }
}

std::expected<void, std::string> Lobby::init(SocketHandler& parrent_socket) {
  LOG("Lobby::init()");
  this->epoll_handler.init();
  this->sockets.emplace_back(
      std::make_unique<SocketHandler>(std::move(parrent_socket)));
  this->epoll_handler.add_socket(sockets[0].get());
  this->sockets[0].get()->socket_type_v = socket_type_e::IPC;
  return {};
}

void Lobby::run() {
  LOG("Lobby::run()");
  while (true) {
    auto result{epoll_handler.wait_for_events(MAX_EVENTS)};
    if (result) {
      for (SocketHandler* handler : *result) {
        while (true) {
          auto result{handler->read_user_input()};
          if (result) {
            auto r = handle_client_cmd(*handler, *result);  // add error log
            if (r) {
            } else {
              break;
            }

          } else {
            break;
          }  // add SocketHandler deletion on "Connection closed"
        }
      }
    } else {
      LOG(result.error());
    }
  }
}
std::expected<void, std::string> Lobby::handle_client_cmd(
    const SocketHandler& client, std::string_view command) {
  LOG("Lobby::handle_client_cmd()");
  using Handler =
      std::function<std::expected<void, std::string>(const SocketHandler&)>;
  LOG(command.data());
  static const std::unordered_map<std::string_view, Handler> command_map = {
      {"test",
       [&client,
        &command](const SocketHandler&) -> std::expected<void, std::string> {
         client.write_to_user(command);
         LOG("test");
         return {};
       }},
      {"socket", [this](const SocketHandler& c) { return accept_socket(c); }},
      {"conn_code",
       [this](const SocketHandler& c) { return send_connection_code(c); }},
      {"close",
       [this](const SocketHandler& c) { return erase_socket_handler(c); }}};
  if (auto it = command_map.find(command); it != command_map.end()) {
    return it->second(client);
  } else {
    client.write_to_user(command);
    return {};
  }
}

std::expected<void, std::string> Lobby::accept_socket(
    const SocketHandler& parrent) {
  LOG("Lobby::accept_socket()");
  auto client_socket = parrent.read_user_input();
  if (client_socket) {
    int flags = fcntl(std::stoi(*client_socket), F_GETFL, 0);
    LOG(std::format("fd= {}  O_NONBLOCK= {}", std::stoi(*client_socket),
                    (flags & O_NONBLOCK)));
    sockets.emplace_back(
        std::make_unique<SocketHandler>(std::stoi(*client_socket)));
    epoll_handler.add_socket(sockets.back().get());
  }
  return {};
}
std::expected<void, std::string> Lobby::send_connection_code(
    const SocketHandler& parrent) {
  LOG("Lobby::send_connection_code()");
  auto conn_code = parrent.read_user_input();
  if (conn_code) {
    auto it = std::ranges::find_if(sockets, [](const auto& c) {
      return c.get()->socket_type() == socket_type_e::CLIENT;
    });
    if (it != sockets.end()) {
      it->get()->write_to_user(*conn_code);
    }
  }
  return {};
}

std::expected<void, std::string> Lobby::erase_socket_handler(
    const SocketHandler& client) {
  std::erase_if(sockets, [&client](const auto& s) {
    return s.get()->get_socket() == client.get_socket();
  });
  return {};
}
}  // namespace bsm
