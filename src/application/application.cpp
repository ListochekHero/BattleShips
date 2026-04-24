#include "application.h"

#include "core/atomic_queue.h"
#include "core/scheduler.h"
#include "net/network_routine.h"
#include "protocol/coroutine_promise.h"
#include "protocol/task_context_types.h"

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <utility>

namespace bsm {

Application::~Application() = default;

Application::Application()
    : network_engine_(network_raw_tasks_, available_task_tags_),
      scheduler_(available_task_tags_) {}

std::expected<ConnectionView, Error> Application::init(end_point_e socket_type,
                                                       int parrent_socket) {
  network_engine().set_message_handler(
      [this](CommandContext context) { return handle_client_cmd(context); });
  auto init_result = network_engine().init_engine(socket_type, parrent_socket);
  if (!init_result) {
    return std::unexpected(std::move(init_result)
                               .error()
                               .add_context("Unalbe to init Application"));
  }
  scheduler_.init();
  scheduler().add_executor(
      task_tag_e::NETWORK, [this](std::unique_ptr<TaskContext> context) {
        auto* network_context =
            static_cast<NetworkTaskContext*>(context.get());
        network_engine_.process_client(network_context->slot);
      });
  scheduler_.add_co_task([this]() { return network_co(); },
                         task_tag_e::NETWORK);
  return *init_result;
}

auto Application::network_co() -> bsm_co_handle {
  while (true) {
    size_t* pending_slot{network_raw_tasks_.try_pop()};
    std::unique_ptr<NetworkTaskContext> task_context(
        new NetworkTaskContext(*pending_slot));
    delete pending_slot;
    scheduler().push_task(task_tag_e::NETWORK, std::move(task_context));
    co_await *this;
  }
}

} // namespace bsm
