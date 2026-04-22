#include "lobby.h"

#include "application/lobby.h"
#include "net/socket_routine.h"
#include "protocol/network_defs.h"
#include "utility/logger.h"
#include <cstring>
#include <memory>

using bsm::LOG;

int main(int argc, char* argv[]) {
  char* program_name = strrchr(argv[0], '/');
  program_name++;
  bsm::Logger::instance().init(std::string(program_name));
  LOG("Hello!");
  for (size_t i = 0; i < argc; i++) {
    LOG(argv[i]);
  }
  bsm::SocketHandler sock{std::stoi(argv[1])};
  std::unique_ptr<bsm::Lobby> lobby{std::make_unique<bsm::Lobby>()};
  bsm::LOG("starting lobby!");
  lobby->init(bsm::end_point_e::TO_PARENT, std::stoi(argv[1]));
  lobby->run();
#ifdef DEBUG_LOGS
#endif
}
