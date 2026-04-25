#include "client.h"

#include <iostream>

namespace bsm {

Client::Client()
    : console_handler_(console_raw_tasks_, available_task_tags()) {}

auto Client::init(end_point_e socket_type, int parrent_socket) -> Ev {
  auto init_result = Application::init(socket_type, parrent_socket);
  server_view_ = *init_result;
  scheduler().add_executor(
      task_tag_e::CONSOLE, [this](std::unique_ptr<TaskContext> context) {
        auto* console_context = static_cast<ConsoleTaskContext*>(context.get());
        handle_input({.client_view = ConnectionView{},
                      .message = {.payload = console_context->user_input}});
      });
  scheduler().add_co_task([this]() { return console_co(); },
                          task_tag_e::CONSOLE);
  return {};
}

auto Client::console_co() -> bsm_co_handle {
  while (true) {
    std::string* user_input{console_raw_tasks_.try_pop()};
    std::unique_ptr<ConsoleTaskContext> task_context(
        new ConsoleTaskContext(std::move(*user_input)));
    delete user_input;
    scheduler().push_task(task_tag_e::CONSOLE, std::move(task_context));
    co_await *this;
  }
}

void Client::run() {
  scheduler().add_and_run_producer([this]() { network_engine().run(); });
  scheduler().add_and_run_producer([this]() { console_handler_.run(); });
  scheduler().run_workers();
  scheduler().get_co_by_tag(task_tag_e::SCHEDULER).resume();
}

auto Client::handle_input(const CommandContext& context) -> CommandStatus {
  CommandInfo cmd_info = bsm::Dispatcher::dispatch(context.message);
  switch (cmd_info.scope) {
  case command_scope_e::BROADCAST:
    network_engine().send_message_to(server_view_, {{context.message.payload}});
    return {};
  case command_scope_e::NONE:
  case command_scope_e::LOCAL:
  case command_scope_e::NETWORK:
    break;
  }
  auto client_action = filter_variant<ClientAction>(cmd_info.parsed_cmd);
  if (!client_action) {
    return {
        .command_status_v = cmd_se::CONTINUE,
        .error = std::move(client_action)
                     .error()
                     .add_context("Command not allowed in this context"),
    };
  }
  return std::visit(
      [&](auto&& lobby_action) -> CommandStatus {
        return execute_action(lobby_action, context);
      },
      *client_action);
}

auto Client::handle_client_cmd(const CommandContext& context) -> CommandStatus {
  LOG(std::format("Command to handle: {}", context.message.payload));
  CommandInfo action_to_execute = bsm::Dispatcher::dispatch(context.message);
  auto client_action =
      filter_variant<ClientAction>(action_to_execute.parsed_cmd);
  if (!client_action) {
    return {
        .command_status_v = cmd_se::CONTINUE,
        .error = std::move(client_action)
                     .error()
                     .add_context("Command not allowed in this context"),
    };
  }
  return std::visit(
      [&](auto&& lobby_action) -> CommandStatus {
        return execute_action(lobby_action, context);
      },
      *client_action);
}

auto Client::execute_action(const Quit& /*unused*/,
                            const CommandContext& /*unused*/) -> CommandStatus {
  std::cout << "->_<-" << '\n';
  return {.command_status_v = cmd_se::CONTINUE};
}
auto Client::execute_action(const PrintAble& /*unused*/,
                            const CommandContext& context) -> CommandStatus {

  std::cout << context.message.payload << '\n';
  return {.command_status_v = cmd_se::CONTINUE};
}

} // namespace bsm
