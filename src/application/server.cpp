#include "server.h"

#include "core/dispatcher.h"
#include "protocol/lobby/lobby_types.h"
#include "utility/data_storage.h"
#include "utility/error.h"
#include "utility/utility.h"
#include <csignal>
#include <expected>
#include <sys/wait.h>

namespace bsm {

auto Server::init(end_point_e socket_type, int socket) -> std::optional<Error> {
  auto init_result = Application::init_app(socket_type, socket);
  if (!init_result) {
    return std::move(init_result)
        .error()
        .add_context("Unable to init Server:: failed to init Application");
  }
  return std::nullopt;
}

void Server::run() {
  scheduler().add_and_run_producer(
      [this]() -> void { network_engine().run_event_loop(); });
  scheduler().run_workers();
  scheduler().coroutine_loop();
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

auto Server::handle_action(const ActionContext& context) -> ActionResult {
  ActionInfo action_info = bsm::Dispatcher::dispatch(context.received_message);
  auto server_action = filter_variant<ServerAction>(action_info.variant);
  if (!server_action) {
    return {
        .conn_status = ConnectionStatus::KEEP,
        .error = std::move(server_action)
                     .error()
                     .add_context("Action in not allowed in this context"),
    };
  }
  return std::visit(
      [&](const auto& action_type) -> ActionResult {
        return execute_action(action_type, context);
      },
      *server_action);
}

auto Server::execute_action(const CreateLobby& /*unused*/,
                            const ActionContext& context) -> ActionResult {
  auto lobby_view{create_lobby()};
  if (!lobby_view) {
    lobby_view.error().add_context(
        "Unable to create lobby: failed to request lobby");
    return {
        .conn_status = ConnectionStatus::KEEP,
        .error = std::move(lobby_view.error()),
        .user_code = user_error_e::CANT_CREATE_LOBBY,
    };
  }
  if (auto send_error = init_lobby(*lobby_view)) {
    send_error->add_context("Unable to create lobby:L failed to init lobby");
    return {
        .conn_status = ConnectionStatus::KEEP,
        .error = std::move(send_error),
        .user_code = user_error_e::CANT_CREATE_LOBBY,
    };
  }
  return perform_transfer(*lobby_view, context);
}

auto Server::create_lobby() -> std::expected<LobbyView, Error> {
  auto spawn_result = bsm::LobbyManager::spawn_lobby();
  if (!spawn_result) {
    return std::unexpected(
        std::move(spawn_result)
            .error()
            .add_context("Unable to create lobby: failed to spawn lobby"));
  }
  auto attach_result = network_engine().attach_socket(
      spawn_result->control_socket, end_point_e::TO_LOBBY);
  if (!attach_result) {
    return std::unexpected(std::move(attach_result)
                               .error()
                               .add_context("Unable to create lobby: failed to "
                                            "attach lobby to NetworkEngine"));
  }
  auto lobby_view = lobby_manager_.attach(spawn_result->pid, *attach_result);
  if (!lobby_view) {
    return std::unexpected(std::move(lobby_view)
                               .error()
                               .add_context("Unable to create lobby: failed to "
                                            "attach lobby to LobbyManager"));
  }
  return lobby_view;
}

auto Server::init_lobby(const LobbyView& lobby_view) -> std::optional<Error> {
  auto send_error = network_engine().send_message_to(
      lobby_view.control_connection,
      {
          .payload = {std::to_string(lobby_view.lobby_id)},
          .type = message_type_e::LOBBY_ID,
      });
  if (send_error) {
    return send_error->add_context(
        "Unable to init lobby: failed to send lobby id");
  }
  return std::nullopt;
}

auto Server::perform_transfer(const LobbyView& recipient, ActionContext context)
    -> ActionResult {
  auto transfer_result{
      network_engine().transfer(recipient.control_connection,
                                context.pending_view),
  };
  if (!transfer_result.destination_conn_preserved) {
    lobby_manager_.kill_lobby(recipient.lobby_id);
  }
  if (transfer_result.source_conn_preserved) {
    return {.conn_status = ConnectionStatus::KEEP};
  }
  return {.conn_status = ConnectionStatus::RELEASE};
}

auto Server::execute_action(const JoinLobby& /*unused*/,
                            const ActionContext& context) -> ActionResult {
  auto parse_result = JoinLobbyCode::parse(context.received_message.payload);
  if (!parse_result) {
    parse_result.error().add_context(
        "Unable to join lobby: failed to parse connection code");
    return {
        .conn_status = ConnectionStatus::KEEP,
        .error = std::move(parse_result).error(),
        .user_code = user_error_e::CANT_JOIN_LOBBY,
    };
  }
  auto lobby_view = lobby_manager_.find(parse_result->int_code);
  if (!lobby_view) {
    lobby_view.error().add_context(
        "Unable to join lobby: failed to find lobby");
    return {
        .conn_status = ConnectionStatus::KEEP,
        .error = std::move(lobby_view).error(),
        .user_code = user_error_e::CANT_JOIN_LOBBY,
    };
  }
  return perform_transfer(*lobby_view, context);
}

auto Server::execute_action(const ChatMessage& /*unused*/,
                            const ActionContext& context) -> ActionResult {
  auto parse_result = parse(context.received_message.payload);
  if (!parse_result) {
    parse_result.error().add_context(
        "Unable to process chat message: failed to parse incoming message");
    return {
        .conn_status = ConnectionStatus::KEEP,
        .error = std::move(parse_result).error(),
    };
  }
  network_engine().send_message(
      {.payload = {*parse_result}, .type = message_type_e::PRINTABLE},
      [](const auto& meta) -> auto {
        return meta.type == end_point_e::TO_CLIENT;
      });
  return {.conn_status = ConnectionStatus::KEEP};
}

auto Server::execute_action(const GeneralAction& /*unused*/,
                            const ActionContext& context) -> ActionResult {
  auto send_error = network_engine().send_message_to(
      context.pending_view,
      {
          .payload = {user_message(user_error_e::UNKNOWN_COMMAND)},
          .type = message_type_e::PRINTABLE,
      });
  if (send_error) {
    send_error->add_context(
        "Unable to process general action: failed to send answer");
    return {
        .conn_status = ConnectionStatus::RELEASE,
        .error = std::move(send_error),
    };
  }
  return {.conn_status = ConnectionStatus::KEEP};
}

} // namespace bsm
