#include "application.h"
#include "config.h"
#include "logger.h"
#include "socket_routine.h"
#include "utility.h"
#include <cstddef>
#include <exception>
#include <expected>
#include <memory>

namespace bsm {

Ev Server::init() {
  if (auto result = add_to_socket_pool(); !result) {
    LOG(result.error().message);
    return std::unexpected(Error{"Unable to add init socket to pool"});
  };
  return this->sockets[0]
      ->setup_listener(Config::instance().get_line("port"))
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{"Cant setup listener"});
      })
      .and_then([this]() -> Ev { return init_epoll_wrapper(); });
}

Ev Server::init(int parrent_socket) {
  if (auto result = add_to_socket_pool(parrent_socket); !result) {
    LOG(result.error().message);
    return std::unexpected(Error{"Unable to add parent socket to pool"});
  }
  this->sockets[0]->set_socket_type(socket_type_e::IPC);
  return init_epoll_wrapper();
}

Ev Server::emplace_socket_to_pool(std::unique_ptr<SocketHandler> socket_ptr) {
  try {
    this->sockets.emplace_back(std::move(socket_ptr));
    sockets.back()->set_occupied_slot(sockets.size() - 1);
  } catch (const std::exception& e) {
    LOG(e.what());
    return std::unexpected(Error{"Unable to emplace new socket handler"});
  }
  return {};
}

Ev Server::add_to_socket_pool() {
  return emplace_socket_to_pool(std::make_unique<SocketHandler>())
      .transform_error([](auto&& error) {
        LOG(error.message);
        error.message = "Unable to increase socket pool size";
        return error;
      });
}

Ev Server::add_to_socket_pool(int socket_fd) {
  return emplace_socket_to_pool(std::make_unique<SocketHandler>(socket_fd))
      .transform_error([](auto&& error) {
        LOG(error.message);
        error.message = "Unable to add given socket to socket pool";
        return error;
      });
}
void Server::run_deferred_actions() {
  for (auto& action : deferred_actions) {
    action->run(*this);
  }
  deferred_actions.clear();
}
void Server::run() {
  while (true) {
    handle_zombie_pocesses();
    [[maybe_unused]]
    auto __ = epoll_handler.wait_for_events(MAX_EVENTS)
                  .and_then([this](auto&& event_slots) -> Ev {
                    process_events(event_slots);
                    return {};
                  })
                  .or_else([](auto&& error) -> Ev {
                    LOG(error.message);
                    LOG("Cant get events");
                    return {};
                  });
  }
  run_deferred_actions();
}

Ev Server::init_epoll() {
  return this->epoll_handler.init()
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{"Cant init epoll_handler"});
      })
      .and_then([this]() -> Ev {
        return this->epoll_handler.add_socket(*sockets[0])
            .or_else([](auto&& error) -> Ev {
              LOG(error.message);
              return std::unexpected(Error{"Cant add socket to epoll"});
            });
      });
}

Ev Server::init_epoll_wrapper() {
  return init_epoll().or_else([](auto&& error) -> Ev {
    LOG(error.message);
    return std::unexpected(Error{"Unable to init epoll"});
  });
}

void Server::process_events(std::vector<size_t>& event_slots) {
  for (size_t slot : event_slots) {
    SocketHandler& handler{*sockets[slot]};
    if (handler.get_socket_type() == socket_type_e::SERVER) {
      process_server_socket(handler);
    } else {
      process_client_socket(handler);
    }
  }
}

void Server::process_server_socket(SocketHandler& handler) {
  [[maybe_unused]]
  auto __ = handler.accept_connections()
                .and_then([this](auto&& new_clients) -> Ev {
                  process_new_clients(new_clients);
                  return {};
                })
                .or_else([](auto&& error) -> Ev {
                  LOG(error.message);
                  LOG("Cant accept new connections");
                  return {};
                });
}
void Server::process_client_socket(SocketHandler& handler) {
  CommandStatus cmd_status{cmd_se::CONTINUE};
  while (cmd_status.command_status_v == cmd_se::CONTINUE &&
         handler.socket_status_v == socket_status_e::ALIVE) {
    auto read_result = handler.read_user_input();
    if (!read_result) {
      LOG(read_result.error().message);
      break;
    }
    CommandContext context{handler, read_result.value()};
    cmd_status = process_message(context);
  }
}

CommandStatus Server::process_message(CommandContext& context) {
  switch (context.message.status) {
  case bsm::message_status_e::EMPTY:
  case bsm::message_status_e::WOULDBLOCK:
    return CommandStatus{cmd_se::TERMINATE};
  case message_status_e::DISCONNECTED:
    return free_socket_handler(context);
  case bsm::message_status_e::DATA:
    break;
  }
  CommandStatus cmd_status = handle_client_cmd(context);
  if (cmd_status.error) {
    LOG(cmd_status.error->message);
    LOG("Unable to handle client command: " + context.message.payload);
    if (auto result = send_error_reply(context.client, *cmd_status.error);
        !result)
      LOG(result.error().message);
  }
  return cmd_status;
}

Ev Server::send_error_reply(const SocketHandler& client, Error& error) {
  return client.write_to_user({{user_message(error.user_code)}})
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{"Unable to send an answer to user"});
      });
}

CommandStatus Server::handle_client_cmd(CommandContext& context) {
  LOG(std::format("Command to handle: {}", context.message.payload));
  for (const auto& cmd : commands) {
    if (match_cmd(cmd, context.message)) {
      return (this->*cmd.handler)(context);
    }
  }
  return general_command(context);
}

std::expected<LobbyProcess, Error> Server::spawn_lobby_process() {
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
    _exit(127);
  }
  close(sv[0]);
  SocketHandler child_handler{sv[1]};
  return LobbyProcess{pid, std::move(child_handler)};
}

CommandStatus Server::create_lobby(const CommandContext& context) {
  if (auto result = epoll_handler.remove_socket(context.client); !result) {
    LOG(result.error().message);
    return CommandStatus{
        cmd_se::CONTINUE,
        Error{"Unable to remove socket from epoll", us_e::CANT_CREATE_LOBBY}};
  }
  auto lobby_result = spawn_lobby_process();
  if (!lobby_result) {
    LOG(lobby_result.error().message);
    return CommandStatus{
        cmd_se::CONTINUE,
        Error{"Unable to spawn child process", us_e::CANT_CREATE_LOBBY}};
  }
  int64_t child_id{generate_conn_code()};
  auto [it, inserted] = lobbies.try_emplace(child_id, std::move(*lobby_result));
  if (!inserted) {
    kill(lobby_result->pid, SIGKILL);
    return CommandStatus{
        cmd_se::CONTINUE,
        Error("Unable to emplace lobby into map", us_e::CANT_CREATE_LOBBY)};
  }
  if (auto result = init_child(it->second.ipc_socket, child_id, context.client);
      !result) {
    LOG(result.error().message);
    kill(it->second.pid, SIGKILL);
    lobbies.erase(it->first);
    return CommandStatus{
        cmd_se::TERMINATE,
        Error{"Unalbe to initialize child process", us_e::CANT_CREATE_LOBBY}};
  }
  context.client.socket_status_v = socket_status_e::TRANSFERED;
  deferred_actions.emplace_back(
      std::make_unique<CleanupSHP_Slot>(context.client.get_occupied_slot()));
  return CommandStatus{cmd_se::TERMINATE};
}

Ev Server::init_child(const SocketHandler& child_socket, int64_t child_id,
                      SocketHandler& client) {
  return child_socket
      .write_to_user(
          {{"\\socket"}, message_type_e::SOCKET, client.get_socket()})
      .or_else([](const Error& error) -> Ev {
        LOG(error.message);
        return std::unexpected(
            Error{"Unable to send client socket to lobby process,",
                  us_e::CANT_CREATE_LOBBY});
      })
      .and_then([&child_socket, &child_id]() -> Ev {
        return child_socket
            .write_to_user(
                {{std::to_string(child_id)}, message_type_e::CONN_CODE})
            .or_else([](const Error& error) -> Ev {
              LOG(error.message);
              return std::unexpected(
                  Error{"Unable to send connection code to lobby process,",
                        us_e::CANT_CREATE_LOBBY});
            });
      });
}

CommandStatus Server::free_socket_handler(const CommandContext& context) {
  if (auto result = epoll_handler.remove_socket(context.client); !result) {
    LOG(result.error().message);
    LOG("Unable to remove socket from epoll before closing");
  }
  context.client.reset_to_empty();
  avaiable_slots.emplace_back(context.client.get_occupied_slot());
  return CommandStatus{cmd_se::TERMINATE};
}

void Server::handle_zombie_pocesses() {
  int status;
  pid_t pid;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (WIFEXITED(status)) {
      LOG(std::format("Child process exited: pid={}, status={}", pid,
                      WEXITSTATUS(status)));
    }
  }
}

void Server::cleanup_slot(size_t slot) {
  ReadResult result{};
  free_socket_handler((CommandContext{*sockets[slot], result}));
}

void Server::process_new_clients(std::vector<int>& new_clients) {
  for (int client : new_clients) {
    SocketHandler& client_handler = [&]() -> SocketHandler& {
      if (!avaiable_slots.empty()) {
        size_t slot = avaiable_slots.back();
        avaiable_slots.pop_back();
        sockets[slot]->reset_with_new(client);
        return *sockets[slot];
      } else {
        auto client_handler{std::make_unique<SocketHandler>(client)};
        client_handler->set_occupied_slot(sockets.size());
        sockets.emplace_back(std::move(client_handler));
        return *sockets.back();
      }
    }();
    if (auto result = epoll_handler.add_socket(client_handler); !result) {
      LOG(result.error().message);
      LOG("Unable to add new client to epoll");
      if (auto r = send_error_reply(client_handler, result.error()); !r) {
        LOG(r.error().message);
      }
      avaiable_slots.emplace_back(client_handler.get_occupied_slot());
      client_handler.reset_to_empty();
    }
  }
}

CommandStatus Server::accept_socket_from_parent(const CommandContext& context) {
  if (context.message.socket) {
    if (auto result = add_to_socket_pool(context.message.socket.value());
        !result) {
      LOG(result.error().message);
      return CommandStatus{cmd_se::TERMINATE}; // finish!
    }
    drop(epoll_handler.add_socket(*sockets.back())
             .or_else([this](auto&& error) -> Ev {
               LOG(error.message);
               sockets.pop_back();
               return std::unexpected(
                   Error{"Unable to add socket from parent into epoll"});
             })
             .and_then([this]() -> Ev {
               return sockets.back()
                   ->write_to_user({{"Connected to lobby"}})
                   .or_else([](auto&& error) -> Ev {
                     LOG(error.message);
                     return std::unexpected(Error{
                         "lobby process - unable to communicate with client"});
                   });
             }));
  }
  return {cmd_se::CONTINUE};
}

CommandStatus Server::send_connection_code(const CommandContext& context) {
  auto conn_code = ConnectionCode::parse(context.message.payload);
  if (auto result = sockets.back()->write_to_user({{conn_code->string_code}});
      !result) {
    LOG(result.error().message);
    return CommandStatus{
        cmd_se::CONTINUE,
        Error{"lobby process - unable to send connection code to client"}};
  }
  return CommandStatus{cmd_se::CONTINUE};
}

CommandStatus Server::join_lobby(const CommandContext& context) {
  auto parse_result = ConnectionCode::parse(context.message.payload);
  if (!parse_result) {
    LOG(parse_result.error().message);
    return CommandStatus{cmd_se::CONTINUE,
                         Error{"Unable to get connection code for lobby",
                               us_e::CANT_JOIN_LOBBY}};
  }
  int64_t child_id{parse_result.value().int_code};
  auto lobby = found_lobby(child_id);
  if (!lobby) {
    LOG(lobby.error().message);
    return CommandStatus{
        cmd_se::CONTINUE,
        Error{"Unable to find lobby with given id", us_e::CANT_JOIN_LOBBY}};
  }
  if (auto result =
          init_child(lobby.value()->ipc_socket, child_id, context.client);
      !result) {
    LOG(result.error().message);
    return CommandStatus{
        cmd_se::CONTINUE,
        Error{"Unable to pass client socket to lobby", us_e::CANT_JOIN_LOBBY}};
  }
  context.client.socket_status_v = socket_status_e::TRANSFERED;
  return CommandStatus{cmd_se::TERMINATE};
}

CommandStatus
Server::general_command([[maybe_unused]] const CommandContext& context) {
  return CommandStatus{cmd_se::CONTINUE,
                       Error("Unknown command", us_e::UNKNOWN_COMMAND)};
}

std::expected<LobbyProcess*, Error> Server::found_lobby(int64_t child_id) {
  auto it{lobbies.find(child_id)};
  if (it != lobbies.end())
    return &(it->second);
  return std::unexpected(Error("No such lobby"));
}

CommandStatus Server::chat_message(const CommandContext& context) {
  auto parse_result = parse(context.message.payload);
  if (!parse_result) {
    LOG(parse_result.error().message);
    return CommandStatus{cmd_se::CONTINUE,
                         Error{"Unable to parse client chat message"}};
  }
  const std::string chat_message{std::move(parse_result.value())};
  bool had_error{false};
  for (auto& receiver : sockets) {
    switch (receiver->get_socket_type()) {
    case bsm::socket_type_e::IPC:
    case bsm::socket_type_e::SERVER:
    case bsm::socket_type_e::UNKNOWN:
      continue;
    case bsm::socket_type_e::CLIENT:
    case bsm::socket_type_e::SPECTATOR:
      if (auto result = receiver->write_to_user(
              {{context.client.nick_name + " " + chat_message}});
          !result) {
        LOG(result.error().message);
        had_error = true;
      }
    }
  }
  return !had_error ? CommandStatus{cmd_se::CONTINUE}
                    : CommandStatus{cmd_se::CONTINUE,
                                    Error{"Unable to send chat message to "
                                          "every client currently active "}};
}

const std::array<Server::Command, 6> Server::commands = {
    {{.handler = &Server::create_lobby, .match = {{"\\create"}, std::nullopt}},
     {.handler = &Server::free_socket_handler,
      .match = {{"\\close"}, std::nullopt}},
     {.handler = &Server::accept_socket_from_parent,
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
