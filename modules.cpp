#include "modules.h"

namespace bsm {

void NetworkModule::setup(Scheduler& scheduler) {
  scheduler.add_executor("network_task",
                         [this](std::unique_ptr<TaskContext> context) {
                           NetworkTaskContext* network_context =
                               static_cast<NetworkTaskContext*>(context.get());
                           net_engine_.process_client(network_context->slot);
                         });
  size_t slot =
      scheduler.add_co_task([this]() { return net_engine_.run_co(); });
  scheduler.schedule_co_task(slot, [&scheduler](
                                       std::coroutine_handle<> te_handle) {
    co_handle_type co_handle =
        co_handle_type::from_address(te_handle.address());
    while (true) {
      co_handle.resume();
      scheduler.push_task("network_task", std::make_unique<NetworkTaskContext>(
                                              co_handle.promise().latest_slot));
    }
  });
}
} // namespace bsm
