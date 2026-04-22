#include "server.h"

#include "utility/data_storage.h"

namespace bsm {

Ev Server::init(end_point_e socket_type, int parrent_socket) {
  Application::init(socket_type, parrent_socket);
  return {};
}

Ev Server::init() {
  network_engine().set_message_handler(
      [this](CommandContext context) { return handle_client_cmd(context); });
  if (auto init_result = network_engine().init_engine(end_point_e::LISTENER, 0);
      !init_result) {
    return std::unexpected(
        std::move(init_result).error().add_context("Unalbe to init Server"));
  }
  network_engine().attach_to_scheduler(scheduler());
  return {};
}

void Server::run() {
  scheduler().add_and_run_producer([this]() { network_engine().run(); });
  scheduler().run_workers();
  scheduler().get_co_by_tag(task_tag_e::SCHEDULER).resume();
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

CommandStatus Server::handle_client_cmd(const CommandContext& context) {
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
  auto lobby_ipc = network_engine().attach_socket(proc->control_socket,
                                                  end_point_e::TO_LOBBY);
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
      [](const auto& meta) { return meta.type == end_point_e::TO_CLIENT; });
  return {cmd_se::CONTINUE};
}

CommandStatus Server::execute_action(const GeneralAction&,
                                     const CommandContext& context) {
  network_engine().send_message_to(
      context.client_view,
      {{user_message(us_e::UNKNOWN_COMMAND)}, message_type_e::PRINTABLE});
  return {cmd_se::CONTINUE};
}

} // namespace bsm
