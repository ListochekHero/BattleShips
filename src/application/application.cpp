#include "application.h"

#include "utility/data_storage.h"
#include "utility/logger.h"

#include <chrono>
#include <expected>
#include <iostream>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <variant>

namespace bsm {

NetworkEngine& Application::network_engine() { return network_engine_; }
Dispatcher& Application::dispatcher() { return dispatcher_; }
Scheduler& Application::scheduler() { return scheduler_; }

Ev Server::init() {
  network_engine().set_message_handler(
      [this](CommandContext context) { return handle_client_cmd(context); });
  if (auto init_result = network_engine().init_engine(end_point_e::SERVER);
      !init_result) {
    return std::unexpected(
        std::move(init_result).error().add_context("Unalbe to init Server"));
  }
  network_engine().attach_to_scheduler(scheduler());
  return {};
}

void Server::run() {
  scheduler().run();
  std::this_thread::sleep_for(std::chrono::seconds(1000));
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

CommandStatus Server::handle_client_cmd(CommandContext& context) {
  LOG(std::format("Command to handle: {}", context.message.payload));
  CommandInfo command_info = dispatcher().dispatch(context.message);
  auto server_action = filter_variant<ServerAction>(command_info.parsed_cmd);
  if (!server_action) {
    return {cmd_se::CONTINUE,
            std::move(server_action)
                .error()
                .add_context("Command not allowed in this context")};
  }
  return std::visit(
      [&](const auto& action_type) -> CommandStatus {
        return execute_action(action_type, context);
      },
      *server_action);
}

CommandStatus Server::execute_action(const CreateLobby&,
                                     const CommandContext& context) {
  auto lobby_view{request_lobby()};
  if (!lobby_view) {
    return {cmd_se::CONTINUE, std::move(lobby_view.error()),
            us_e::CANT_CREATE_LOBBY};
  }
  network_engine().send_message_to(
      lobby_view->control_connection,
      {{std::to_string(lobby_view->lobby_id)}, message_type_e::LOBBY_ID});

  // return CommandStatus{cmd_se::CONTINUE, std::move(result).error()};
  return network_engine().transfer(lobby_view->control_connection,
                                   context.client_view);
}

std::expected<LobbyView, Error> Server::request_lobby() {
  std::optional<Error> error;
  auto proc = lobby_manager_.spawn_lobby();
  if (!proc) {
    error = proc.error();
  }
  auto lobby_ipc =
      network_engine().attach_socket(proc->control_socket, end_point_e::LOBBY);
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

CommandStatus Server::execute_action(const JoinLobby&,
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
  return network_engine().transfer(lobby_view->control_connection,
                                   context.client_view);
}

CommandStatus Server::execute_action(const ChatMessage&,
                                     const CommandContext& context) {
  auto parse_result = parse(context.message.payload);
  if (!parse_result) {
    parse_result.error().add_context("Unable to parse client chat message");
    return CommandStatus{cmd_se::CONTINUE, std::move(parse_result).error()};
  }
  network_engine().send_message(
      {{*parse_result}, message_type_e::PRINTABLE},
      [](const auto& meta) { return meta.type == end_point_e::CLIENT; });
  return {cmd_se::CONTINUE};
}

CommandStatus Server::execute_action(const GeneralAction&,
                                     const CommandContext& context) {
  network_engine().send_message_to(
      context.client_view,
      {{user_message(us_e::UNKNOWN_COMMAND)}, message_type_e::PRINTABLE});
  return {cmd_se::CONTINUE};
}

Ev Lobby::init(int parrent_socket, end_point_e socket_type) {
  network_engine().set_message_handler(
      [this](CommandContext context) { return handle_client_cmd(context); });
  auto init_result = network_engine().init_engine(parrent_socket, socket_type);
  if (!init_result) {
    return std::unexpected(
        std::move(init_result)
            .error()
            .add_context("Unable to init Lobby with parent socket"));
  }
  parent_view_ = *init_result;
  network_engine().attach_to_scheduler(scheduler());
  return {};
}

void Lobby::run() {
  scheduler().run();
  std::this_thread::sleep_for(std::chrono::seconds(1000));
}

CommandStatus Lobby::handle_client_cmd(CommandContext& context) {
  LOG(std::format("Command to handle: {}", context.message.payload));
  auto action_to_execute = dispatcher().dispatch(context.message);
  auto lobby_action = filter_variant<LobbyAction>(action_to_execute.parsed_cmd);
  if (!lobby_action) {
    return {cmd_se::CONTINUE,
            std::move(lobby_action)
                .error()
                .add_context("Command not allowed in this context")};
  }
  return std::visit(
      [&](auto&& lobby_action) -> CommandStatus {
        return execute_action(lobby_action, context);
      },
      *lobby_action);
}

CommandStatus Lobby::execute_action(const AcceptSocket&,
                                    const CommandContext& context) {
  if (!context.message.socket) {
    network_engine().send_message_to(
        parent_view_, {{"No socket found in message"}, message_type_e::ERROR});
    return CommandStatus{cmd_se::CONTINUE};
  }
  auto client_view = network_engine().attach_socket(*context.message.socket,
                                                    end_point_e::CLIENT);
  if (!client_view) {
    client_view.error().add_context(
        "Unable to accept client socket from parent");
    return CommandStatus{cmd_se::CONTINUE, std::move(client_view).error()};
  }
  network_engine().send_message_to(
      *client_view, {{"Connected to lobby\n", "Connection code is: \n", "\t",
                      std::to_string(lobby_id_)},
                     message_type_e::PRINTABLE});
  return {cmd_se::CONTINUE};
}

CommandStatus Lobby::execute_action(const LobbyIdSetter&,
                                    const CommandContext& context) {
  lobby_id_ = std::strtol(context.message.payload.c_str(), nullptr, 10);
  return {cmd_se::CONTINUE};
}

Ev Client::init(end_point_e socket_type) {
  network_engine().set_message_handler(
      [this](const CommandContext& context) { return handle_input(context); });
  console_handler_.set_input_handler(
      [this](std::string user_cmd) { return register_user_input(user_cmd); });
  auto init_result = network_engine().init_engine(socket_type);
  if (!init_result) {
    return std::unexpected(
        std::move(init_result)
            .error()
            .add_context("Unable to init Client and connect to Server"));
  }
  server_view_ = *init_result;
  scheduler().add_executor(
      "console_task", [this](std::unique_ptr<TaskContext> context) {
        ConsoleTaskContext* console_context =
            static_cast<ConsoleTaskContext*>(context.get());
        handle_input(
            {ConnectionView{}, {.payload = console_context->user_input}});
      });
  network_engine().attach_to_scheduler(scheduler());
  console_handler_.attach_to_scheduler(scheduler());
  return {};
}

void Client::run() {
  scheduler().run();
  std::this_thread::sleep_for(std::chrono::seconds(1000));
}

CommandStatus Client::handle_input(const CommandContext& context) {
  CommandInfo cmd_info = dispatcher().dispatch(context.message);
  switch (cmd_info.scope) {
  case command_scope_e::BROADCAST:
    network_engine().send_message_to(server_view_, {{context.message.payload}});
    return {};
  }
  auto client_action = filter_variant<ClientAction>(cmd_info.parsed_cmd);
  if (!client_action) {
    return {cmd_se::CONTINUE,
            std::move(client_action)
                .error()
                .add_context("Command not allowed in this context")};
  }
  return std::visit(
      [&](auto&& lobby_action) -> CommandStatus {
        return execute_action(lobby_action, context);
      },
      *client_action);
}

CommandStatus Client::handle_client_cmd(const CommandContext& context) {
  LOG(std::format("Command to handle: {}", context.message.payload));
  CommandInfo action_to_execute = dispatcher().dispatch(context.message);
  auto client_action =
      filter_variant<ClientAction>(action_to_execute.parsed_cmd);
  if (!client_action) {
    return {cmd_se::CONTINUE,
            std::move(client_action)
                .error()
                .add_context("Command not allowed in this context")};
  }
  return std::visit(
      [&](auto&& lobby_action) -> CommandStatus {
        return execute_action(lobby_action, context);
      },
      *client_action);
}

CommandStatus Client::execute_action(const Quit&,
                                     const CommandContext& context) {
  std::cout << "-><-" << std::endl;
  return {cmd_se::CONTINUE};
}
CommandStatus Client::execute_action(const PrintAble&,
                                     const CommandContext& context) {

  std::cout << context.message.payload << std::endl;
  return {cmd_se::CONTINUE};
}
void Client::register_user_input(std::string user_input) {
  std::lock_guard lock{m_};
  // input_queue_.push(std::move(user_input));
  cv_.notify_one();
}

CommandStatus Client::handle_local_cmd(ParsedCommand command_variant) {
  auto local_action = filter_variant<LocalClientAction>(command_variant);
  if (!local_action) {
    return {cmd_se::CONTINUE,
            std::move(local_action)
                .error()
                .add_context("Command not allowed in this context")};
  }
  return std::visit(
      [&](auto&& local_action) -> CommandStatus {
        return execute_local_action(local_action);
      },
      *local_action);
}

CommandStatus Client::execute_local_action(const Quit&) {
  std::cout << "-><-" << std::endl;
  return {cmd_se::CONTINUE};
}

} // namespace bsm
