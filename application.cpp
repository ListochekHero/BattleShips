#include "application.h"
#include "config.h"
#include "lobby_manager.h"
#include "logger.h"
#include "network_routine.h"
#include "socket_routine.h"
#include "utility.h"
#include <cstddef>
#include <exception>
#include <expected>
#include <memory>

namespace bsm {

Ev Server::init() {
  net_engine_.init();
  net_engine_.set_message_handler(
      [this](CommandContext context) { return handle_client_cmd(context); });
}

Ev Server::init(int parrent_socket, end_point_type_e socket_type) {
  net_engine_.init(parrent_socket, socket_type);
  net_engine_.set_message_handler(
      [this](CommandContext context) { return handle_client_cmd(context); });
}

void Server::run() { net_engine_.run(); }

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
  return LobbyProcess{pid, sv[1]};
}

void Server::execute_action(const CreateLobby& action,
                            const CommandContext& context) {
  auto proc = lobby_manager_.spawn_lobby();
  if (!proc) {
    LOG(proc.error().message);
    return
  }
  ConnectionView lobby_ipc =
      net_engine_.attach(proc->control_socket, end_point_type_e::LOBBY);
  auto lobby_handler = lobby_manager_.attach(proc->pid, lobby_ipc);
  if (!lobby_handler) {
    LOG(lobby_handler.error().message);
    LOG("Unable");
  }
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
  auto conn_code = JoinLobbyCode::parse(context.message.payload);
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
  auto parse_result = JoinLobbyCode::parse(context.message.payload);
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
  net_engine.set_status_to(context.client, socket_status_e::TRANSFERED);
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

} // namespace bsm
