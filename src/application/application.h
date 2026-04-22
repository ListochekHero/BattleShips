#ifndef APPLICATION_H
#define APPLICATION_H

#include <coroutine>
#include <cstddef>
#include <expected>
#include <sys/wait.h>

#include "core/atomic_queue.h"
#include "core/dispatcher.h"
#include "core/scheduler.h"
#include "net/network_routine.h"
#include "protocol/coroutine_promise.h"
#include "protocol/network_types.h"
#include "utility/error.h"
#include "utility/utility.h"

namespace bsm {

class Application {
public:
  bool await_ready() { return false; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) {
    return scheduler_.get_co_by_tag(task_tag_e::SCHEDULER);
  };
  void await_resume() {}

  Application();
  std::expected<ConnectionView, Error> init(end_point_e socket_type,
                                            int parrent_socket);
  virtual void run() = 0;
  bsm_co_handle network_co();
  virtual CommandStatus handle_client_cmd(const CommandContext& context) = 0;
  virtual ~Application();

protected:
  NetworkEngine& network_engine() { return network_engine_; }
  Dispatcher& dispatcher() { return dispatcher_; }
  Scheduler& scheduler() { return scheduler_; }
  AtomicQueue<task_tag_e>& available_task_tags() {
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
