#include "server.h"

#include "utility/data_storage.h"
#include <sys/wait.h>

namespace bsm {

auto Server::v_init(end_point_e socket_type, int parrent_socket) -> Ev {
  return Application::init(socket_type, parrent_socket)
      .transform([](auto) -> void {});
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

auto Server::handle_client_cmd(const CommandContext& context) -> CommandStatus {
  LOG(std::format("Command to handle: {}", context.message.payload));
  CommandInfo command_info = bsm::Dispatcher::dispatch(context.message);
  auto server_action = filter_variant<ServerAction>(command_info.parsed_cmd);
  if (!server_action) {
    return {
        .command_status_v = cmd_se::CONTINUE,
        .error = std::move(server_action)
                     .error()
                     .add_context("Command not allowed in this context"),
    };
  }
  return std::visit(
      [&](const auto& action_type) -> CommandStatus {
        return execute_action(action_type, context);
      },
      *server_action);
}

auto Server::execute_action(const CreateLobby& /*unused*/,
                            const CommandContext& context) -> CommandStatus {
  auto lobby_view{request_lobby()};
  if (!lobby_view) {
    return {
        .command_status_v = cmd_se::CONTINUE,
        .error = std::move(lobby_view.error()),
        .user_code = us_e::CANT_CREATE_LOBBY,
    };
  }
  network_engine().send_message_to(
      lobby_view->control_connection,
      {
          .payloads = {std::to_string(lobby_view->lobby_id)},
          .msg_type = message_type_e::LOBBY_ID,
      });

  // return CommandStatus{cmd_se::CONTINUE, std::move(result).error()};
  return network_engine().transfer(lobby_view->control_connection,
                                   context.client_view);
}

auto Server::request_lobby() -> std::expected<LobbyView, Error> {
  std::optional<Error> error;
  auto proc = bsm::LobbyManager::spawn_lobby();
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

auto Server::execute_action(const JoinLobby& /*unused*/,
                            const CommandContext& context) -> CommandStatus {
  auto parse_result = JoinLobbyCode::parse(context.message.payload);
  if (!parse_result) {
    parse_result.error().add_context("Unable to parse connection code");
    return {
        .command_status_v = cmd_se::CONTINUE,
        .error = std::move(parse_result).error(),
        .user_code = us_e::CANT_JOIN_LOBBY,
    };
  }
  auto lobby_view = lobby_manager_.find(parse_result->int_code);
  if (!lobby_view) {
    lobby_view.error().add_context("Unable to find lobby with given id");
    return {
        .command_status_v = cmd_se::CONTINUE,
        .error = std::move(lobby_view).error(),
        .user_code = us_e::CANT_JOIN_LOBBY,
    };
  }
  return network_engine().transfer(lobby_view->control_connection,
                                   context.client_view);
}

auto Server::execute_action(const ChatMessage& /*unused*/,
                            const CommandContext& context) -> CommandStatus {
  auto parse_result = parse(context.message.payload);
  if (!parse_result) {
    parse_result.error().add_context("Unable to parse client chat message");
    return {
        .command_status_v = cmd_se::CONTINUE,
        .error = std::move(parse_result).error(),
    };
  }
  network_engine().send_message(
      {.payloads = {*parse_result}, .msg_type = message_type_e::PRINTABLE},
      [](const auto& meta) -> auto {
        return meta.type == end_point_e::TO_CLIENT;
      });
  return {.command_status_v = cmd_se::CONTINUE};
}

auto Server::execute_action(const GeneralAction& /*unused*/,
                            const CommandContext& context) -> CommandStatus {
  network_engine().send_message_to(
      context.client_view,
      {
          .payloads = {user_message(us_e::UNKNOWN_COMMAND)},
          .msg_type = message_type_e::PRINTABLE,
      });
  return {.command_status_v = cmd_se::CONTINUE};
}

} // namespace bsm
