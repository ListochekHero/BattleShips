#include "main.h"

int main(int argc, char* argv[]) {
  char* program_name = strrchr(argv[0], '/');
  program_name++;
  bsm::Logger::instance().init(program_name);
  bsm::Config config;
#ifdef DEBUG_LOGS
  bsm::LOG("DEBUG Enabled!");
#endif
  std::unique_ptr<bsm::Server> server{std::make_unique<bsm::Server>()};
  if (argc > 1) {
    bsm::SocketHandler parrent_ipc{std::stoi(argv[1])};
    bsm::LOG("Starting Lobby!");
    server->init(parrent_ipc);
  } else {
    bsm::LOG("Starting Server!");
    server->init();
  }
  server->run();
}
