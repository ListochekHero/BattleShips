// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <vector>
#include "client.h"

#include "core/dispatcher.h"
#include "protocol/message_types.h"
#include "protocol/task_context_types.h"
#include "utility/error.h"
#include "utility/logger.h"
#include "utility/utility.h"

#include <format>
#include <iostream>
#include <memory>
#include <utility>

namespace bsm {

Client::Client()
    : console_handler_(console_raw_tasks_, available_task_tags()) {}

auto Client::init(end_point_e socket_type, int socket) -> std::optional<Error> {
  auto init_result = Application::init_app(socket_type, socket);
  if (!init_result) {
    return std::move(init_result)
        .error()
        .add_context("Unable to init Client:: failed to init Application");
  }
  server_view_ = *init_result;
  if (auto registration_error{register_console_task()}) {
    std::move(registration_error)
        ->add_context("Unable to init Client: failed to register console task");
  }
  return std::nullopt;
}

void Client::run() {
  scheduler().add_and_run_producer(
      [this]() -> void { network_engine().run_event_loop(); });
  scheduler().add_and_run_producer([this]() { console_handler_.run(); });
  scheduler().run_workers();
  scheduler().get_co_handle_by_tag(task_tag_e::SCHEDULER).resume();
}

auto Client::register_console_task() -> std::optional<Error> {
  auto add_error = scheduler().add_task_executor(
      task_tag_e::CONSOLE,
      [this](std::unique_ptr<TaskContext> context) -> void {
        auto* console_context = static_cast<ConsoleTaskContext*>(context.get());
        handle_action({
            .pending_view = ConnectionView{},
            .received_message = {.payload = console_context->input_},
        });
      });
  if (add_error) {
    return std::move(add_error)->add_context(
        "Unalbe to register console task: failed to add executor "
        "for task CONSOLE");
  }
  add_error = scheduler().add_co_handle(
      [this]() -> bsm_co_handle { return console_co(); }, task_tag_e::CONSOLE);
  if (add_error) {
    return std::move(add_error)->add_context(
        "Unalbe to register console task: failed to add coroutine "
        "for task CONSOLE");
  }
  return std::nullopt;
}

auto Client::console_co() -> bsm_co_handle { // NOLINT
  while (true) {
    std::string* user_input{console_raw_tasks_.try_pop()};
    std::unique_ptr<ConsoleTaskContext> task_context =
        std::make_unique<ConsoleTaskContext>(std::move(*user_input));
    delete user_input;
    scheduler().push_task(task_tag_e::CONSOLE, std::move(task_context));
    co_await yield_to_scheduler(); // NOLINT
  }
}

auto Client::handle_action(const ActionContext& context) -> ActionResult {
  ActionInfo action_info = bsm::Dispatcher::dispatch(context.received_message);
  switch (action_info.scope) {
  case bsm::action_scope_e::BROADCAST:
    return send_input_to_server(context);
  case action_scope_e::NONE:
  case action_scope_e::LOCAL:
  case action_scope_e::NETWORK:
    break;
  }
  auto client_action = filter_variant<ClientAction>(action_info.variant);
  if (!client_action) {
    return {
        .conn_status = ConnectionStatus::KEEP,
        .error = std::move(client_action)
                     .error()
                     .add_context("Action is not allowed in this context"),
    };
  }
  return std::visit(
      [&](auto&& client_action) -> ActionResult {
        return execute_action(client_action, context);
      },
      *client_action);
}

auto Client::send_input_to_server(const ActionContext& context)
    -> ActionResult {
  if (auto send_error{
          network_engine().send_message_to(
              server_view_, {.payloads = {context.received_message.payload}}),
      }) {
    return {
        .error =
            Error{
                .backtrace =
                    {
                        "Unable to send input to server : failed to "
                        "send message",
                    },
            },
        .user_code = user_error_e::GENERIC, // add error message for this case
    };
  }
  return {};
}

auto Client::execute_action(const PrintMessage& /*unused*/,
                            const ActionContext& context) -> ActionResult {

  std::cout << context.received_message.payload << '\n';
  return {.conn_status = ConnectionStatus::KEEP};
}

auto Client::execute_action(const Quit& /*unused*/,
                            const ActionContext& /*unused*/) -> ActionResult {
  std::cout << "->_<-" << '\n';
  return {.conn_status = ConnectionStatus::KEEP};
}

} // namespace bsm
