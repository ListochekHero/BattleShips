#include "console_routine.h"
#include <iostream>
#include <string>

namespace bsm {

void ConsoleHandler::run() {
  std::string user_cmd;
  while (std::getline(std::cin, user_cmd)) {
    if (user_cmd == "quit")
      break;
  }
}

} // namespace bsm
