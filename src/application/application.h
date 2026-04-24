#ifndef APPLICATION_H
#define APPLICATION_H

#include <coroutine>
#include <cstddef>
#include <expected>

#include "core/atomic_queue.h"
#include "core/dispatcher.h"
#include "core/scheduler.h"
#include "net/network_routine.h"
#include "protocol/coroutine_promise.h"
#include "protocol/network_types.h"
#include "utility/error.h"
#include "utility/utility.h"

namespace bsm {
enum class end_point_e : uint8_t;
class Application {
public:
  static auto await_ready() -> bool { return false; }
  auto await_suspend(std::coroutine_handle<> /*unused*/)
      -> std::coroutine_handle<> {
    return scheduler_.get_co_by_tag(task_tag_e::SCHEDULER);
  };
  void await_resume() {}

  Application();
  auto init(end_point_e socket_type, int parrent_socket)
      -> std::expected<ConnectionView, Error>;
  virtual void run() = 0;
  auto network_co() -> bsm_co_handle;
  virtual auto handle_client_cmd(const CommandContext& context)
      -> CommandStatus = 0;
  virtual ~Application();

protected:
  auto network_engine() -> NetworkEngine& { return network_engine_; }
  auto dispatcher() -> Dispatcher& { return dispatcher_; }
  auto scheduler() -> Scheduler& { return scheduler_; }
  auto available_task_tags() -> AtomicQueue<task_tag_e>& {
    return available_task_tags_;
  }

private:
  NetworkEngine network_engine_;
  AtomicQueue<size_t> network_raw_tasks_;
  Dispatcher dispatcher_;
  Scheduler scheduler_;
  AtomicQueue<task_tag_e> available_task_tags_;
};

} // namespace bsm

#endif
