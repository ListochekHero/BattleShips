#include "client.h"

#include <iostream>

namespace bsm {

Client::Client()
    : console_handler_(console_raw_tasks_, available_task_tags()) {}

Ev Client::init(end_point_e socket_type, int parrent_socket) {
  auto init_result = Application::init(socket_type, parrent_socket);
  server_view_ = *init_result;
  scheduler().add_executor(
      task_tag_e::CONSOLE, [this](std::unique_ptr<TaskContext> context) {
        ConsoleTaskContext* console_context =
            static_cast<ConsoleTaskContext*>(context.get());
        handle_input(
            {.client_view=ConnectionView{}, .message={.payload = console_context->user_input}});
      });
  scheduler().add_co_task([this]() { return console_co(); },
                          task_tag_e::CONSOLE);
  return {};
}

bsm_co_handle Client::console_co() {
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
  std::cout << "->_<-" << std::endl;
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
