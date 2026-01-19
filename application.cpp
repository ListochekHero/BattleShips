#include "application.h"

namespace bsm {

Ev Server::init() {
  this->sockets.emplace_back(std::make_unique<SocketHandler>());
  this->sockets[0].get()->setup_listenter(Config::instance().get_line("port"));
  init_epoll();
  return {};
}
Ev Server::init(SocketHandler&& parrent_socket) {
  this->sockets.emplace_back(
      std::make_unique<SocketHandler>(std::move(parrent_socket)));
  this->sockets[0].get()->set_socket_type(socket_type_e::IPC);
  init_epoll();
  return {};
}

void Server::run() {
  while (true) {
    handle_zombie_pocesses();
    epoll_handler.wait_for_events(MAX_EVENTS)
        .and_then([this](auto&& events) -> Ev {
          process_events(events);
          return {};
        })
        .transform_error(with_log("Cant get events"));
  }
}

Ev Server::init_epoll() {
  this->epoll_handler.init();
  this->epoll_handler.add_socket(sockets[0].get());
  return {};
}

void Server::process_events(std::vector<SocketHandler*>& events) {
  for (SocketHandler* handler : events) {
    if (handler->get_socket_type() == socket_type_e::SERVER) {
      process_server_socket(handler);
    } else {
      process_client_socket(handler);
    }
  }
}

void Server::process_server_socket(SocketHandler* handler) {
  handler->accept_connections()
      .and_then([this](auto&& new_clients) -> Ev {
        for (auto& client : new_clients) {
          epoll_handler.add_socket(client.get())
              .and_then([this, &client]() -> Ev {
                sockets.emplace_back(std::move(client));
                return {};
              })
              .transform_error([&client](const auto& error) {
                client.reset(nullptr);
                return log_and_forward(error);
              });
        }
        return {};
      })
      .transform_error(with_log("Cant accept new connections"));
}
void Server::process_client_socket(SocketHandler* handler) {
  bool read_from_socket{true};
  while (read_from_socket) {
    handler->read_user_input()
        .and_then([this, handler, &read_from_socket](auto&& command) -> Ev {
          if (command.status == status_code_e::WOULDBLOCK ||
              command.status == status_code_e::CLOSED) {
            read_from_socket = false;
          } else {
            handle_client_cmd(*handler, command);
          }
          return {};
        })
        .transform_error([&read_from_socket](const auto& error) {
          log_and_forward(error);
          read_from_socket = false;
          return error;
        });
  }
}

void Server::handle_client_cmd(SocketHandler& client,
                               const ReadResult& message) {
  LOG(std::format("Command to handle: {}", message.payload.data()));
  bool gen_comm = true;
  for (const auto& cmd : commands) {
    if (match_cmd(cmd, message)) {
      (this->*cmd.handler)(client, message)
          .and_then([&gen_comm]() -> Ev {
            gen_comm = false;
            return {};
          })
          .or_else([&client](const auto& error) -> Ev {
            client
                .write_to_user(
                    {message_type_e::DEFAULT, {user_message(error.code)}})
                .transform_error(
                    with_log("handle_client_cmd - cant send answer to user"));
            return std::unexpected(log_and_forward(error));
          });
    }
  }
  if (gen_comm) {
    general_command(client, message)
        .or_else([&client](const auto& error) -> Ev {
          return client.write_to_user(
              {message_type_e::DEFAULT, {user_message(error.code)}});
        });
  }
}

std::expected<SocketHandler, Error> Server::spawn_lobby_process() {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, sv) == -1)
    return std::unexpected(make_error(
        er_e::SYSTEM,
        std::format("Failed to create socketpair: {}", c_error_string())));
  pid_t pid = fork();
  if (pid == -1)
    return std::unexpected(make_error(
        er_e::SYSTEM, std::format("Failed to fork: {}", c_error_string())));
  if (pid == 0) {
    close(sv[1]);
    std::string socket_string{std::to_string(sv[0])};
    execl("/home/listochekhero/projects/battleships/build/server", "./lobby",
          socket_string.c_str(), NULL);
    perror("execl failed");
    _exit(127);
  }
  close(sv[0]);
  SocketHandler child_handler{sv[1]};
  return child_handler;
}

Ev Server::create_lobby(SocketHandler& client, const ReadResult& message) {
  epoll_handler.remove_socket(&client).transform_error(log_and_forward);
  auto lobby_result = spawn_lobby_process();
  if (!lobby_result) {
    log_and_forward(lobby_result.error());
    return std::unexpected(make_error(er_e::SYSTEM, "Cant create lobby"));
  }
  int64_t child_id{generate_conn_code()};
  auto it = lobbies.emplace(child_id, std::move(*lobby_result));
  if (!it.second)
    return std::unexpected(make_error(er_e::INTERNAL, "Cant store lobby"));
  init_child(it.first->second, child_id, client)
      .or_else([](const auto& error) -> Ev {
        return std::unexpected(log_and_forward(error));
      })
      .and_then([this, &client, &message]() -> Ev {
        erase_socket_handler(client, message);
        return {};
      });
  return {};
}

Ev Server::init_child(const SocketHandler& child_socket, int64_t child_id,
                      SocketHandler& client) {
  return child_socket
      .write_to_user(
          {message_type_e::SOCKET, {"\\socket"}, client.get_socket()})
      .or_else([](const Error& error) -> Ev {
        return std::unexpected(log_and_forward(error));
      })
      .and_then([&child_socket, &child_id]() -> Ev {
        return child_socket
            .write_to_user(
                {message_type_e::CONN_CODE, {std::to_string(child_id)}})
            .or_else([](const Error& error) -> Ev {
              return std::unexpected(log_and_forward(error));
            });
      });
}

Ev Server::erase_socket_handler(SocketHandler& client,
                                const ReadResult& message) {
  epoll_handler.remove_socket(&client).transform_error(log_and_forward);
  std::erase_if(sockets, [&client](const auto& s) {
    return s.get()->get_socket() == client.get_socket();
  });
  return {};
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

Ev Server::accept_socket(SocketHandler& parrent, const ReadResult& message) {
  if (message.socket) {
    sockets.emplace_back(std::make_unique<SocketHandler>(*message.socket));
    epoll_handler.add_socket(sockets.back().get());
    sockets.back().get()->write_to_user(
        {message_type_e::DEFAULT, {"Connected to lobby"}});
  }
  return {};
}

Ev Server::send_connection_code(SocketHandler& parrent,
                                const ReadResult& message) {

  auto conn_code = ConnectionCode::parse(message.payload);
  auto it = std::ranges::find_if(sockets, [](const auto& c) {
    return c.get()->get_socket_type() == socket_type_e::CLIENT;
  });
  if (it != sockets.end()) {
    it->get()->write_to_user({message_type_e::DEFAULT, {conn_code->string_code}});
  }
  return {};
}

Ev Server::general_command(SocketHandler& client, const ReadResult& message) {
  // in case we didnt found a specific command lets see what else we can do
  if (int64_t child_id{std::stol(parse_conn_code(message.payload))}) {
    // maybe this is a connection code, lets try find lobby for it
    found_lobby(child_id).and_then(
        [this, &client, child_id, &message](auto child_ipc) -> Ev {
          client.remove_cloexec();
          if (!(init_child(*child_ipc, child_id, client))
                   .and_then([this, &client, &message]() -> Ev {
                     erase_socket_handler(client, message);
                     return {};
                   })
                   .transform_error(log_and_forward))
            return std::unexpected(make_error(
                er_e::INTERNAL, "Cant pass socket to child process"));
          return {};
        });
  }
  return std::unexpected(make_error(er_e::INTERNAL, "Unknown command"));
}

std::expected<SocketHandler*, Error> Server::found_lobby(int64_t child_id) {
  auto it{lobbies.find(child_id)};
  if (it != lobbies.end()) return &(it->second);
  return std::unexpected(make_error(er_e::NOT_FOUND, "No such lobby"));
}

const std::array<Server::Command, 5> Server::commands = {
    {{.handler = &Server::create_lobby, .match = {{"\\create"}, std::nullopt}},
     {.handler = &Server::erase_socket_handler,
      .match = {{"\\close"}, std::nullopt}},
     {.handler = &Server::accept_socket,
      .match = {{"\\socket"}, message_type_e::SOCKET}},
     {.handler = &Server::send_connection_code,
      .match = {{"\\conn_code"}, message_type_e::CONN_CODE}},
     {.handler = &Server::join_lobby,
      .match = {{"\\join"}, message_type_e::CONN_CODE}}}};

bool Server::match_cmd(const Command& cmd, const ReadResult& msg) {
  if (cmd.match.msg_type && cmd.match.msg_type == msg.msg_type) return true;
  for (auto alias : cmd.match.text_aliases)
    if (msg.payload.starts_with(alias)) return true;
  return false;
}

}  // namespace bsm
