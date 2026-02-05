#include "main.h"
#include "application.h"
#include "config.h"
#include "logger.h"

int main(int argc, char* argv[]) {
  char* program_name = strrchr(argv[0], '/');
  program_name++;
  bsm::Logger::instance().init(program_name);
  bsm::Config config;
#ifdef DEBUG_LOGS
  bsm::LOG("DEBUG Enabled!");
#endif
  bsm::Ev init_result;
  std::unique_ptr<bsm::Server> server{std::make_unique<bsm::Server>()};
  if (argc > 1) {
    // bsm::SocketHandler parrent_ipc{std::stoi(argv[1])};
    bsm::LOG("Starting Lobby!");
    init_result = server->init(std::stoi(argv[1]));
  } else {
    bsm::LOG("Starting Server!");
    init_result = server->init();
  }
  if (!init_result) {
    bsm::LOG(init_result.error().message);
    return 1;
  }
  server->run();
  return 0;
}
