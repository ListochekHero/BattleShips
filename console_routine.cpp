#include "console_routine.h"
#include <iostream>
#include <string>

namespace bsm {

void ConsoleHandler::run() {
  std::string user_cmd;
  while (std::getline(std::cin, user_cmd)) {
    on_input_callback(std::move(user_cmd));
    if (user_cmd == "\\quit")
      break;
  }
}
void ConsoleHandler::set_input_handler(InputHandler h) {
  on_input_callback = h;
}

} // namespace bsm
