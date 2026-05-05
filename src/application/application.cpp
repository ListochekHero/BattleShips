#include "application.h"

#include "core/atomic_queue.h"
#include "core/scheduler.h"
#include "net/network_routine.h"
#include "protocol/coroutine_promise.h"
#include "protocol/network_types.h"
#include "protocol/task_context_types.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <utility>

namespace bsm {

Application::Application()
    : network_engine_(network_raw_tasks_, available_task_tags_),
      scheduler_(available_task_tags_) {}

Application::~Application() = default;

auto Application::init_app(end_point_e socket_type, int socket)
    -> std::expected<ConnectionView, Error> {
  auto init_result{init_network(socket_type, socket)};
  if (!init_result) {
    return std::unexpected(init_result.error().add_context(
        "Unable to init Application: failed to init network"));
  }
  if (auto init_error{init_scheduler()}) {
    return std::unexpected(
        std::move(init_error)
            ->add_context(
                "Unable to init Application: failed to init scheduler"));
  }
  if (auto registration_error{register_network_task()}) {
    std::move(registration_error)
        ->add_context(
            "Unable to init Application: failed to register network task");
  }
  return *init_result;
}

auto Application::init_network(end_point_e socket_type, int socket)
    -> std::expected<ConnectionView, Error> {
  network_engine().set_message_handler(
      [this](const ActionContext& context) -> ActionResult {
        return handle_action(context);
      });
  auto init_result = network_engine().init_engine(socket_type, socket);
  if (!init_result) {
    return std::unexpected(
        std::move(init_result)
            .error()
            .add_context("Unalbe to init network: failed to init engine"));
  }
  return init_result;
}

auto Application::init_scheduler() -> std::optional<Error> {
  if (auto init_error{scheduler_.init()}) {
    return std::move(init_error)
        ->add_context("Unalbe to init scheduler: failed to init scheduler");
  }
  return std::nullopt;
}

auto Application::register_network_task() -> std::optional<Error> {
  auto add_error = scheduler().add_task_executor(
      task_tag_e::NETWORK,
      [this](std::unique_ptr<TaskContext> context) -> void {
        auto& network_context = static_cast<NetworkTaskContext&>(*context);
        network_engine_.process_connection(
            ConnectionView{network_context.slot_});
      });
  if (add_error) {
    return std::move(add_error)->add_context(
        "Unalbe to register network task: failed to add executor "
        "for task NETWORK");
  }
  add_error = scheduler_.add_co_handle(
      [this]() -> bsm_co_handle { return network_co(); }, task_tag_e::NETWORK);
  if (add_error) {
    return std::move(add_error)->add_context(
        "Unalbe to register network task: failed to add coroutine "
        "for task NETWORK");
  }
  return std::nullopt;
}

auto Application::network_co() -> bsm_co_handle { // NOLINT
  while (true) {
    size_t* pending_slot{network_raw_tasks_.try_pop()};
    std::unique_ptr<NetworkTaskContext> task_context{
        std::make_unique<NetworkTaskContext>(*pending_slot),
    };
    delete pending_slot;
    scheduler().push_task(task_tag_e::NETWORK, std::move(task_context));
    co_await yield_to_scheduler(); // NOLINT
  }
}

} // namespace bsm
