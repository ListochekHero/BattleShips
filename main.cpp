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
  bsm::LOG("starting server!");
  server->init();
  server->run();
}
