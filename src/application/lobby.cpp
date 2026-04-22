#include "lobby.h"

namespace bsm {

Ev Lobby::init(end_point_e socket_type, int parrent_socket) {
  auto init_result = Application::init(socket_type, parrent_socket);
  parent_view_ = *init_result;
  return {};
}

void Lobby::run() {
  scheduler().add_and_run_producer([this]() { network_engine().run(); });
  scheduler().run_workers();
  scheduler().get_co_by_tag(task_tag_e::SCHEDULER).resume();
}

CommandStatus Lobby::handle_client_cmd(const CommandContext& context) {
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
                                                    end_point_e::TO_CLIENT);
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

CommandStatus Lobby::execute_action(const ChatMessage&,
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

} // namespace bsm
