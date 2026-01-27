#include "application.h"


namespace bsm {

Ev Server::init() {
  this->sockets.emplace_back(std::make_unique<SocketHandler>());
  return this->sockets[0]
      .get()
      ->setup_listener(Config::instance().get_line("port"))
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{er_e::SYSTEM, "Cant setup listener"});
      })
      .and_then([this]() -> Ev { return init_epoll_wrapper(); });
}

Ev Server::init(SocketHandler&& parrent_socket) {
  this->sockets.emplace_back(
      std::make_unique<SocketHandler>(std::move(parrent_socket)));
  this->sockets[0].get()->set_socket_type(socket_type_e::IPC);
  return init_epoll_wrapper();
}

void Server::run() {
  while (true) {
    handle_zombie_pocesses();
    [[maybe_unused]]
    auto __ = epoll_handler.wait_for_events(MAX_EVENTS)
                  .and_then([this](auto&& events) -> Ev {
                    process_events(events);
                    return {};
                  })
                  .or_else([](auto&& error) -> Ev {
                    LOG(error.message);
                    LOG("Cant get events");
                    return {};
                  });
  }
}

Ev Server::init_epoll() {
  return this->epoll_handler.init()
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{er_e::SYSTEM, "Cant init epoll_handler"});
      })
      .and_then([this]() -> Ev {
        return this->epoll_handler.add_socket(sockets[0].get())
            .or_else([](auto&& error) -> Ev {
              LOG(error.message);
              return std::unexpected(
                  Error{er_e::SYSTEM, "Cant add socket to epoll"});
            });
      });
  return {};
}

Ev Server::init_epoll_wrapper() {
  return init_epoll().or_else([](auto&& error) -> Ev {
    LOG(error.message);
    return std::unexpected(Error{er_e::SYSTEM, "Cant init epoll"});
  });
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
  [[maybe_unused]]
  auto __ = handler->accept_connections()
                .and_then([this](auto&& new_clients) -> Ev {
                  for (auto& client : new_clients) {
                    epoll_handler.add_socket(client.get())
                        .and_then([this, &client]() -> Ev {
                          sockets.emplace_back(std::move(client));
                          return {};
                        })
                        .or_else([&client](auto&& error) -> Ev {
                          LOG(error.message);
                          client.reset(nullptr);
                          return std::unexpected(Error{
                              er_e::SYSTEM, "Cant add new client to epoll"});
                        });
                  }
                  return {};
                })
                .or_else([](auto&& error) -> Ev {
                  LOG(error.message);
                  LOG("Cant accept new connections");
                  return {};
                });
}
void Server::process_client_socket(SocketHandler* handler) {
  status_code_e read_status = status_code_e::DATA;
  while (read_status == status_code_e::DATA) {
    auto read_result = handler->read_user_input();
    if (!read_result) {
      LOG(read_result.error().message);
      break;
    }
    ReadResult message{read_result.value()};
    read_status = message.status;
    if (message.status == status_code_e::DATA)
      handle_client_cmd(*handler, message);
  }
}

void Server::handle_client_cmd(SocketHandler& client,
                               const ReadResult& message) {
  LOG(std::format("Command to handle: {}", message.payload));
  for (const auto& cmd : commands) {
    if (match_cmd(cmd, message)) {
      if (auto command_result = (this->*cmd.handler)(client, message);
          !command_result) {
        [[maybe_unused]]
        auto __ = client
                      .write_to_user(
                          {message_type_e::DEFAULT,
                           {user_message(command_result.error().user_code)}})
                      .or_else([](auto&& error) -> Ev {
                        LOG(error.message);
                        LOG("handle_client_cmd - cant send answer to user");
                        return {};
                      });
      }
      return;
    }
  }
  [[maybe_unused]]
  auto __ = general_command(client, message)
                .or_else([&client](const auto& error) -> Ev {
                  return client
                      .write_to_user({message_type_e::DEFAULT,
                                      {user_message(error.user_code)}})
                      .or_else([](auto&& error) -> Ev {
                        LOG(error.message);
                        LOG("handle_client_cmd - cant send answer to user");
                        return {};
                      });
                });
  return;
}

std::expected<LobbyProcess, Error> Server::spawn_lobby_process() {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, sv) == -1)
    return std::unexpected(
        make_error(er_e::SYSTEM, std::format("Failed to create socketpair: {}",
                                             c_error_string())));
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
  return LobbyProcess{pid, std::move(child_handler)};
}

Ev Server::create_lobby(SocketHandler& client, const ReadResult& message) {
  auto lobby_result = spawn_lobby_process();
  if (!lobby_result) {
    LOG(lobby_result.error().message);
    return std::unexpected(Error{er_e::SYSTEM, "Unable to spawn child process",
                                 us_e::CANT_CREATE_LOBBY});
  }
  int64_t child_id{generate_conn_code()};
  auto it = lobbies.emplace(child_id, std::move(*lobby_result));
  if (!it.second)
    return std::unexpected(Error(er_e::INTERNAL,
                                 "Unable to emplace lobby into map",
                                 us_e::CANT_CREATE_LOBBY));
  if (auto result = init_child(it.first->second.ipc_socket, child_id, client);
      !result) {
    LOG(result.error().message);
    kill(it.first->second.pid, SIGKILL);
    lobbies.erase(it.first);
    return std::unexpected(Error{er_e::INTERNAL,
                                 "Unalbe to initialize child process",
                                 us_e::CANT_CREATE_LOBBY});
  }
  return erase_socket_handler(client, message).or_else([](auto&& error) -> Ev {
    LOG(error.message);
    return std::unexpected(
        Error{er_e::SYSTEM,
              "Unable to erase socket after passing it to lobby process",
              us_e::CANT_CREATE_LOBBY});
  });
}

Ev Server::init_child(const SocketHandler& child_socket, int64_t child_id,
                      SocketHandler& client) {
  return child_socket
      .write_to_user(
          {message_type_e::SOCKET, {"\\socket"}, client.get_socket()})
      .or_else([](const Error& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{
            er_e::SYSTEM, "Unable to send client socket to lobby process,",
            us_e::CANT_CREATE_LOBBY});
      })
      .and_then([&child_socket, &child_id]() -> Ev {
        return child_socket
            .write_to_user(
                {message_type_e::CONN_CODE, {std::to_string(child_id)}})
            .or_else([](const Error& error) -> Ev {
              LOG(error.message);
              return std::unexpected(
                  Error{er_e::SYSTEM,
                        "Unable to send connection code to lobby process,",
                        us_e::CANT_CREATE_LOBBY});
            });
      });
}

Ev Server::erase_socket_handler(SocketHandler& client,
                                [[maybe_unused]] const ReadResult& message) {
  [[maybe_unused]]
  auto __ =
      epoll_handler.remove_socket(&client).or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(
            Error{er_e::SYSTEM,
                  "Unable to remove socket from epoll before deleting"});
      });
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

Ev Server::accept_socket([[maybe_unused]] SocketHandler& parrent,
                         const ReadResult& message) {
  if (message.socket) {
    sockets.emplace_back(std::make_unique<SocketHandler>(*message.socket));
    [[maybe_unused]]
    auto __ =
        epoll_handler.add_socket(sockets.back().get())
            .or_else([this](auto&& error) -> Ev {
              LOG(error.message);
              sockets.pop_back();
              return std::unexpected(Error{
                  er_e::SYSTEM, "Unable to add socket from parent into epoll"});
            })
            .and_then([this]() -> Ev {
              return sockets.back()
                  ->write_to_user(
                      {message_type_e::DEFAULT, {"Connected to lobby"}})
                  .or_else([](auto&& error) -> Ev {
                    LOG(error.message);
                    return std::unexpected(Error{
                        er_e::SYSTEM,
                        "lobby process - unable to communicate with client"});
                  });
            });
  }
  return {};
}

Ev Server::send_connection_code([[maybe_unused]] SocketHandler& parrent,
                                const ReadResult& message) {
  auto conn_code = ConnectionCode::parse(message.payload);
  return sockets.back()
      ->write_to_user({message_type_e::DEFAULT, {conn_code->string_code}})
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(
            Error{er_e::SYSTEM,
                  "lobby process - unable to send connection code to client"});
      });
}
Ev Server::join_lobby(SocketHandler& client, const ReadResult& message) {
  auto parse_result = ConnectionCode::parse(message.payload);
  if (!parse_result) {
    LOG(parse_result.error().message);
    return std::unexpected(Error{er_e::INVALID_ARGS,
                                 "Unable to get connection code for lobby",
                                 us_e::CANT_JOIN_LOBBY});
  }
  int64_t child_id{parse_result.value().int_code};
  auto lobby = found_lobby(child_id);
  if (!lobby) {
    LOG(lobby.error().message);
    return std::unexpected(Error{er_e::INVALID_ARGS,
                                 "Unable to find lobby with given id",
                                 us_e::CANT_JOIN_LOBBY});
  }
  if (auto result = init_child(lobby.value()->ipc_socket, child_id, client);
      !result) {
    LOG(result.error().message);
    return std::unexpected(Error{er_e::INTERNAL,
                                 "Unable to pass client socket to lobby",
                                 us_e::CANT_JOIN_LOBBY});
  }
  return erase_socket_handler(client, message);
}

Ev Server::general_command([[maybe_unused]] SocketHandler& client,
                           [[maybe_unused]] const ReadResult& message) {
  return std::unexpected(Error(er_e::INVALID_ARGS, "Unknown command"));
}

std::expected<LobbyProcess*, Error> Server::found_lobby(int64_t child_id) {
  auto it{lobbies.find(child_id)};
  if (it != lobbies.end())
    return &(it->second);
  return std::unexpected(make_error(er_e::NOT_FOUND, "No such lobby"));
}

Ev Server::chat_message([[maybe_unused]]SocketHandler& client, const ReadResult& message) {
  auto parse_result = parse(message.payload);
  if (!parse_result) {
    LOG(parse_result.error().message);
    return std::unexpected(
        Error{er_e::INTERNAL, "Unable to parse client chat message"});
  }
  const std::string chat_message{std::move(parse_result.value())};
  bool had_error {false};
  for (auto& receiver : sockets) {
    switch (receiver->get_socket_type()) {
    case bsm::socket_type_e::IPC:
    case bsm::socket_type_e::SERVER:
    case bsm::socket_type_e::UNKNOWN:
      continue;
    case bsm::socket_type_e::CLIENT:
    case bsm::socket_type_e::SPECTATOR:
      if (auto result = receiver->write_to_user(
              {message_type_e::DEFAULT, {chat_message}});
          !result) {
        LOG(result.error().message);
        had_error = true;
      }
    }
  }

  return !had_error ? Ev{}:std::unexpected(Error{
            er_e::SYSTEM,
            "Unable to send chat message to every client currently active"});

}

const std::array<Server::Command, 6> Server::commands = {
    {{.handler = &Server::create_lobby, .match = {{"\\create"}, std::nullopt}},
     {.handler = &Server::erase_socket_handler,
      .match = {{"\\close"}, std::nullopt}},
     {.handler = &Server::accept_socket,
      .match = {{"\\socket"}, message_type_e::SOCKET}},
     {.handler = &Server::send_connection_code,
      .match = {{"\\conn_code"}, message_type_e::CONN_CODE}},
     {.handler = &Server::join_lobby,
      .match = {{"\\join"}, message_type_e::CONN_CODE}},
     {.handler = &Server::chat_message, .match = {{"\\msg"}, std::nullopt}}}};

bool Server::match_cmd(const Command& cmd, const ReadResult& msg) {
  if (cmd.match.msg_type && cmd.match.msg_type == msg.msg_type)
    return true;
  for (auto alias : cmd.match.text_aliases)
    if (msg.payload.starts_with(alias))
      return true;
  return false;
}

} // namespace bsm
