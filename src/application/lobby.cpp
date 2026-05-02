// IWYU pragma: no_include <vector>
// IWYU pragma: no_include <string_view>
#include "lobby.h"

#include "protocol/message_defs.h"
#include "protocol/message_types.h"
#include "protocol/network_defs.h"
#include "utility/logger.h"
#include "utility/utility.h"

#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace bsm {

auto Lobby::v_init(end_point_e socket_type, int parrent_socket) -> Ev {
  auto init_result = Application::init(socket_type, parrent_socket);
  parent_view_ = *init_result;
  return {};
}

void Lobby::run() {
  scheduler().add_and_run_producer(
      [this]() -> void { network_engine().run_event_loop(); });
  scheduler().run_workers();
  scheduler().get_co_by_tag(task_tag_e::SCHEDULER).resume();
}

auto Lobby::handle_client_cmd(const ActionContext& context) -> ActionResult {
  LOG(std::format("Command to handle: {}", context.received_message.payload));
  auto action_to_execute = bsm::Dispatcher::dispatch(context.received_message);
  auto lobby_action = filter_variant<LobbyAction>(action_to_execute.parsed_cmd);
  if (!lobby_action) {
    return {
        .conn_status = ConnectionStatus::KEEP,
        .error = std::move(lobby_action)
                     .error()
                     .add_context("Command not allowed in this context"),
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
  if (!context.received_message.socket) {
    network_engine().send_message_to(
        parent_view_, {
                          .payloads = {"No socket found in message"},
                          .msg_type = message_type_e::ERROR,
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
  network_engine().send_message_to(*client_view,
                                   {
                                       .payloads =
                                           {
                                               "Connected to lobby\n",
                                               "Connection code is: \n",
                                               "\t",
                                               std::to_string(lobby_id_),
                                           },
                                       .msg_type = message_type_e::PRINTABLE,
                                   });
  return {.conn_status = ConnectionStatus::KEEP};
}

auto Lobby::execute_action(const LobbyIdSetter& /*unused*/,
                           const ActionContext& context) -> ActionResult {
  lobby_id_ =
      std::strtol(context.received_message.payload.c_str(), nullptr, 10);
  return {.conn_status = ConnectionStatus::KEEP};
}

auto Lobby::execute_action(const ChatMessage& /*unused*/,
                           const ActionContext& context) -> ActionResult {
  auto parse_result = parse(context.received_message.payload);
  if (!parse_result) {
    parse_result.error().add_context("Unable to parse client chat message");
    return {
        .conn_status = ConnectionStatus::KEEP,
        .error = std::move(parse_result).error(),
    };
  }
  network_engine().send_message(
      {.payloads = {*parse_result}, .msg_type = message_type_e::PRINTABLE},
      [](const auto& meta) -> auto { return meta.type == end_point_e::TO_CLIENT; });
  return {.conn_status = ConnectionStatus::KEEP};
}

} // namespace bsm
