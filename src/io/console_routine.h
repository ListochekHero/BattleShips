#ifndef CONSOLE_ROUTINE_H
#define CONSOLE_ROUTINE_H

#include "core/scheduler.h"
#include "interfaces/modules.h"

#include <coroutine>
#include <functional>
#include <string>

namespace bsm {

struct console_promise;
using console_co_handle = std::coroutine_handle<console_promise>;

struct console_promise {
  std::string latest_input{};
  console_co_handle get_return_object() {
    return console_co_handle::from_promise(*this);
  }
  std::suspend_always initial_suspend() noexcept { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  std::suspend_always yield_value(std::string user_input) {
    latest_input = std::move(user_input);
    return {};
  }
  void unhandled_exception() {}
};

class ConsoleHandler : public Module {
public:
  ConsoleHandler(AtomicQueue<std::string>& console_q_,
                 AtomicQueue<task_tag_e>& available_q_)
      : console_raw_tasks_(console_q_), available_task_tags_(available_q_) {};

  // void attach_to_scheduler(Scheduler& scheduler) override;
  void run();
  console_co_handle run_co();
  using InputHandler = std::function<void(std::string)>;
  void set_input_handler(InputHandler h);

private:
  AtomicQueue<std::string>& console_raw_tasks_;
  AtomicQueue<task_tag_e>& available_task_tags_;
  InputHandler on_input_callback;
};

} // namespace bsm

template <>
struct std::coroutine_traits<bsm::console_co_handle, bsm::ConsoleHandler&> {
  using promise_type = bsm::console_promise;
};
#endif
