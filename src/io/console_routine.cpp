#include "console_routine.h"

#include <iostream>
#include <string>

namespace bsm {

void ConsoleHandler::attach_to_scheduler(Scheduler& scheduler) {
  size_t slot = scheduler.add_co_task([this]() { return run_co(); });
  scheduler.schedule_co_task(
      slot, [&scheduler](std::coroutine_handle<> te_handle) {
        console_co_handle co_handle =
            console_co_handle::from_address(te_handle.address());
        while (true) {
          co_handle.resume();
          scheduler.push_task("console_task",
                              std::make_unique<ConsoleTaskContext>(
                                  std::move(co_handle.promise().latest_input)));
        }
      });
}

void ConsoleHandler::run() {
  std::string user_cmd;
  while (std::getline(std::cin, user_cmd)) {
    on_input_callback(std::move(user_cmd));
    if (user_cmd == "\\quit")
      break;
  }
}

console_co_handle ConsoleHandler::run_co() {
  std::string user_input;
  while (std::getline(std::cin, user_input)) {
    co_yield (std::move(user_input));
    if (user_input == "\\quit")
      break;
  }
}

void ConsoleHandler::set_input_handler(InputHandler h) {
  on_input_callback = h;
}

} // namespace bsm
