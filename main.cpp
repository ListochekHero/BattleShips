#include "main.h"
#include "application.h"
#include "config.h"
#include "logger.h"
#include "network_routine.h"
#include <cstdlib>
#include <memory>

int main(int argc, char* argv[]) {
  char* program_name = strrchr(argv[0], '/');
  program_name++;
  bsm::Logger::instance().init(program_name);
  bsm::Config config;
#ifdef DEBUG_LOGS
  bsm::LOG("DEBUG Enabled!");
#endif
  bsm::Ev init_result{};
  std::unique_ptr<bsm::Application> app;
  if (argc > 1) {
    bsm::LOG("Starting Lobby!");
    app = std::make_unique<bsm::Lobby>();
    init_result = app->init(std::stoi(argv[1]), bsm::end_point_e::LOBBY);
  } else {
    app = std::make_unique<bsm::Server>();
    bsm::LOG("Starting Server!");
    init_result = app->init();
  }
  if (!init_result) {
    bsm::LOG(init_result.error().message);
    return EXIT_FAILURE;
  }
  app->run();
  return 0;
}
