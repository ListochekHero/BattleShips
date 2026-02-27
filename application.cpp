#include "application.h"
#include "data_storage.h"
#include "dispatcher.h"
#include "error.h"
#include "lobby_manager.h"
#include "logger.h"
#include "network_routine.h"
#include "socket_routine.h"
#include "utility.h"
#include <expected>
#include <string>
#include <unistd.h>
#include <variant>

namespace bsm {

Dispatcher& Application::dispatcher() { return dispatcher_; }
NetworkEngine& Application::net_engine() { return net_engine_; }

Ev Server::init() {
  net_engine().set_message_handler(
      [this](CommandContext context) { return handle_client_cmd(context); });
  return net_engine().init();
}

void Server::run() { net_engine().run(); }

Ev Server::send_error_reply(const ConnectionView& conn_view,
                            user_error_e error) {
  return net_engine()
      .send_error_message_to(conn_view, error)
      .or_else([](auto&& error) -> Ev {
        return std::unexpected(
            error.add_context("Unable to send an answer to user"));
      });
}

CommandStatus Server::handle_client_cmd(CommandContext& context) {
  LOG(std::format("Command to handle: {}", context.message.payload));
  auto action_to_exe = dispatcher().dispatch(context);
  return std::visit(
      [&](const auto& action_type) -> CommandStatus {
        return execute_action(action_type, context);
      },
      action_to_exe);
}

std::expected<LobbyView, Error> Server::request_lobby() {
  std::optional<Error> error;
  auto proc = lobby_manager_.spawn_lobby();
  if (!proc) {
    error = proc.error();
  }
  auto lobby_ipc =
      net_engine().attach(proc->control_socket, end_point_e::LOBBY);
  if (!lobby_ipc) {
    error = lobby_ipc.error();
  }
  auto lobby_handler = lobby_manager_.attach(proc->pid, *lobby_ipc);
  if (!lobby_handler) {
    error = lobby_handler.error();
  }
  if (error) {
    return std::unexpected(
        error->add_context("Unable to setup new lobby for request"));
  }
  return lobby_handler;
}

CommandStatus Server::execute_action(const CreateLobby& action_type,
                                     const CommandContext& context) {
  auto lobby_view{request_lobby()};
  if (!lobby_view) {
    if (auto r = net_engine().send_error_message_to(context.client_view,
                                                    us_e::CANT_CREATE_LOBBY);
        !r)
      LOG(r.error().full_report());
    return CommandStatus{cmd_se::CONTINUE, std::move(lobby_view).error()};
  }
  if (auto result = net_engine().send_message_to(
          lobby_view->control_connection,
          {{std::to_string(lobby_view->lobby_id)}, message_type_e::LOBBY_ID});
      !result) {
    return CommandStatus{cmd_se::CONTINUE, std::move(result).error()};
  }
  return net_engine().transfer(lobby_view->control_connection,
                               context.client_view);
}

CommandStatus Server::execute_action(const JoinLobby& action_type,
                                     const CommandContext& context) {
  auto parse_result = JoinLobbyCode::parse(context.message.payload);
  if (!parse_result) {
    parse_result.error().add_context("Unable to parse connection code");
    return CommandStatus{cmd_se::CONTINUE, std::move(parse_result).error(),
                         us_e::CANT_JOIN_LOBBY};
  }
  auto lobby_view = lobby_manager_.find(parse_result->int_code);
  if (!lobby_view) {
    lobby_view.error().add_context("Unable to find lobby with given id");
    return CommandStatus{cmd_se::CONTINUE, std::move(lobby_view).error(),
                         us_e::CANT_JOIN_LOBBY};
  }
  return net_engine().transfer(lobby_view->control_connection,
                               context.client_view);
}

CommandStatus Server::execute_action(const ChatMessage& action_type,
                                     const CommandContext& context) {
  auto parse_result = parse(context.message.payload);
  if (!parse_result) {
    parse_result.error().add_context("Unable to parse client chat message");
    return CommandStatus{cmd_se::CONTINUE, std::move(parse_result).error()};
  }
  const std::string chat_message{std::move(parse_result).value()};
  net_engine().send_message({{*parse_result}}, [](const auto& meta) {
    return meta.type == end_point_e::CLIENT;
  });
}

CommandStatus Server::execute_action(const GeneralAction& action_type,
                                     const CommandContext& context) {
  auto report = net_engine().send_message_to(
      context.client_view, {{user_message(us_e::UNKNOWN_COMMAND)}});
  return {cmd_se::CONTINUE};
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

Ev Lobby::init(int parrent_socket, end_point_e socket_type) {
  auto attach_result = net_engine().attach(parrent_socket, socket_type);
  if (!attach_result) {
    attach_result.error().add_context(
        "Unable to init Lobby with parent socket");
    return std::unexpected(std::move(attach_result).error());
  }
  parent_view_ = *attach_result;
  return {};
}
void Lobby::run() {}

CommandStatus Lobby::execute_action(const AcceptSocket& action_type,
                                    const CommandContext& context) {
  if (!context.message.socket) {
    net_engine().send_message_to(
        parent_view_, {{"No socket found in message"}, message_type_e::ERROR});
    return CommandStatus{cmd_se::CONTINUE};
  }
  auto client_view =
      net_engine().attach(*context.message.socket, end_point_e::CLIENT);
  if (!client_view) {
    client_view.error().add_context(
        "Unable to accept client socket from parent");
    return CommandStatus{cmd_se::CONTINUE, std::move(client_view).error()};
  }
  net_engine().send_message_to(
      *client_view, {{"Connected to lobby\n", "Connection code is: \n", "\t",
                      std::to_string(lobby_id_)}});
  return {cmd_se::CONTINUE};
}

CommandStatus Lobby::execute_action(const LobbyIdSetter& action_type,
                                    const CommandContext& context) {
  lobby_id_ = std::strtol(context.message.payload.c_str(), nullptr, 10);
  return {cmd_se::CONTINUE};
}

CommandStatus Server::execute_action(const Quit& action_type,
                                     const CommandContext& context) {
  return {cmd_se::CONTINUE};
}
CommandStatus Server::execute_action(const AcceptSocket& action_type,
                                     const CommandContext& context) {
  return {cmd_se::CONTINUE};
}
CommandStatus Server::execute_action(const LobbyIdSetter& action_type,
                                     const CommandContext& context) {
  return {cmd_se::CONTINUE};
}

CommandStatus Server::execute_action(const NotAllowed& action_type,
                                     const CommandContext& context) {
  return {cmd_se::CONTINUE};
}

} // namespace bsm
