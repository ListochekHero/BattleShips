// IWYU pragma: no_include <vector>
// IWYU pragma: no_include <string_view>
#include "lobby.h"

#include "core/dispatcher.h"
#include "protocol/message_defs.h"
#include "protocol/message_types.h"
#include "protocol/network_defs.h"
#include "protocol/network_types.h"
#include "utility/logger.h"
#include "utility/utility.h"

#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace bsm {

auto Lobby::init(end_point_e socket_type, int socket) -> std::optional<Error> {
  auto init_result = Application::init_app(socket_type, socket);
  if (!init_result) {
    return std::move(init_result)
        .error()
        .add_context("Unable to init Lobby:: failed to init Application");
  }
  parent_view_ = *init_result;
  return std::nullopt;
}

void Lobby::run() {
  scheduler().add_and_run_producer(
      [this]() -> void { network_engine().run_event_loop(); });
  scheduler().run_workers();
  scheduler().get_co_handle_by_tag(task_tag_e::SCHEDULER).resume();
}

auto Lobby::handle_action(const ActionContext& context) -> ActionResult {
  auto action_info = bsm::Dispatcher::dispatch(context.received_message);
  auto lobby_action = filter_variant<LobbyAction>(action_info.variant);
  if (!lobby_action) {
    return {
        .conn_status = ConnectionStatus::KEEP,
        .error = std::move(lobby_action)
                     .error()
                     .add_context("Action is not allowed in this context"),
    };
  }
  return std::visit(
      [&](auto&& lobby_action) -> ActionResult {
        return execute_action(lobby_action, context);
      },
      *lobby_action);
}

auto Lobby::execute_action(const AcceptSocket& /*unused*/,
                           const ActionContext& context) -> ActionResult {
  if (!context.received_message
           .socket) { // Need to work on Server<->Lobby protocol
    network_engine().send_message_to(
        parent_view_, {
                          .payloads = {"No socket found in message"},
                          .type = message_type_e::ERROR,
                      });
    return {.conn_status = ConnectionStatus::KEEP};
  }
  auto client_view = network_engine().attach_socket(
      *context.received_message.socket, end_point_e::TO_CLIENT);
  if (!client_view) {
    client_view.error().add_context(
        "Unable to accept client socket from parent");
    return {
        .conn_status = ConnectionStatus::KEEP,
        .error = std::move(client_view).error(),
    };
  }
  if (auto greeting_error{send_join_lobby_message(*client_view)}) {
    greeting_error->add_context(
        "Unable to accept socket: failed to send greeting message");
    return {.conn_status = ConnectionStatus::KEEP,
            .error = std::move(greeting_error)};
  }
  return {.conn_status = ConnectionStatus::KEEP};
}

auto Lobby::send_join_lobby_message(ConnectionView recipient_view)
    -> std::optional<Error> {
  if (auto send_error = network_engine().send_message_to(
          recipient_view, {
                              .payloads =
                                  {
                                      "Connected to lobby\n",
                                      "Connection code is: \n",
                                      "\t",
                                      std::to_string(lobby_id_),
                                  },
                              .type = message_type_e::PRINTABLE,
                          })) {
    return send_error->add_context(
        "Unable to send join lobby message: failed to send message");
  }
  return std::nullopt;
}

auto Lobby::execute_action(const SetLobbyId& /*unused*/,
                           const ActionContext& context) -> ActionResult {
  lobby_id_ =
      std::strtol(context.received_message.payload.c_str(), nullptr, 10);
  return {.conn_status = ConnectionStatus::KEEP};
}

auto Lobby::execute_action(const ChatMessage& /*unused*/,
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
      {.payloads = {*parse_result}, .type = message_type_e::PRINTABLE},
      [](const auto& meta) -> auto {
        return meta.type == end_point_e::TO_CLIENT;
      });
  return {.conn_status = ConnectionStatus::KEEP};
}

} // namespace bsm
