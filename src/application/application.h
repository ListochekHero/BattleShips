#ifndef APPLICATION_H
#define APPLICATION_H

// IWYU pragma: begin_exports
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <expected>

#include "core/atomic_queue.h"
#include "core/dispatcher.h"
#include "core/scheduler.h"
#include "net/network_routine.h"
#include "protocol/coroutine_promise.h"
#include "protocol/network_types.h"
#include "utility/error.h"
#include "utility/utility.h"
// IWYU pragma: end_exports

namespace bsm {
enum class end_point_e : uint8_t;
class Application {
public:
  Application();
  virtual void run() = 0;
  auto yield_to_scheduler() { return AppAwaiter(*this); }
  virtual ~Application();

protected:
  auto init(end_point_e socket_type, int parrent_socket)
      -> std::expected<ConnectionView, Error>;
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

  struct AppAwaiter {
    Application& application_;
    static auto await_ready() -> bool { return false; }
    auto await_suspend(std::coroutine_handle<> /*unused*/)
        -> std::coroutine_handle<> {
      return application_.scheduler_.get_co_by_tag(task_tag_e::SCHEDULER);
    };
    void await_resume() {}
  };
};

} // namespace bsm

#endif
