#include "console_routine.h"

#include "core/atomic_queue.h"
#include "core/scheduler.h"

#include <utility>

#include <iostream>
#include <string>

namespace bsm {

void ConsoleHandler::run() {
  std::string user_cmd;
  while (std::getline(std::cin, user_cmd)) {
    auto* ptr{new std::string(std::move(user_cmd))};
    console_raw_tasks_.push(ptr);
    available_task_tags_.push(new task_tag_e(task_tag_e::CONSOLE));
    if (*ptr == "\\quit") {
      break;
    }
  }
}

} // namespace bsm
