// IWYU pragma: no_include <optional>
// IWYU pragma: no_include <vector>
#include "client.h"

#include "core/dispatcher.h"
#include "protocol/message/message_types.h"
#include "protocol/network/network_defs.h"
#include "protocol/task/task_context_types.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <iostream>
#include <memory>
#include <utility>
#include <variant>

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
  scheduler().coroutine_loop();
}

auto Client::register_console_task() -> std::optional<Error> {
  auto add_error = scheduler().add_task_executor(
      task_tag_e::CONSOLE, [this](Task::ContextVariant context) -> void {
        auto* input_string_ptr = std::get_if<std::string>(&context);
        handle_action({
            .pending_view = ConnectionView{},
            .received_message = {.payload = std::move(*input_string_ptr)},
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
    auto user_input{console_raw_tasks_.mmanager_try_pop()};
    if (user_input.has_value()) {
      scheduler().push_task(task_tag_e::CONSOLE, std::move(*user_input));
    }
    co_await yield_to_scheduler(); // NOLINT
  }
}

auto Client::handle_action(const ActionContext& context) -> ActionResult {
  ActionInfo action_info = bsm::Dispatcher::dispatch(context.received_message);
  switch (action_info.scope) {
  case bsm::action_scope_e::BROADCAST: {
    return handle_broadcast_action(context);
  }
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

auto Client::handle_broadcast_action(const ActionContext& context)
    -> ActionResult {
  if (auto send_error{send_input_to_server(context.received_message)}) {
    send_error->add_context(
        "Unable to handle broadcast action: failed to send input to Server. "
        "Trying to reconnect to Server...");
    LOG(send_error->full_report());
    success_or_terminate(
        network_engine().reset_base_connection(end_point_e::TO_SERVER));
    return {
        .error = std::move(send_error)
                     ->add_context("Succesfully reconnected to Server"),
    };
  }
  return {};
}

auto Client::send_input_to_server(const ReceivedMessage& message_to_send)
    -> std::optional<Error> {
  if (auto send_error{
          network_engine().send_message_to(
              server_view_, {.payload = {message_to_send.payload}}),
      }) {
    return send_error->add_context("Unable to send input to server : failed to "
                                   "send message");
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
