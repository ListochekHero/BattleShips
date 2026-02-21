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
  if (auto result = bsm::Logger::instance().init(program_name); !result) {
    std::cout << "Unable to init Logger";
    return EXIT_FAILURE;
  }
  bsm::Config config;
#ifdef DEBUG_LOGS
  bsm::LOG("DEBUG Enabled!");
#endif
  bsm::Ev init_result{};
  std::unique_ptr<bsm::Application> app;
  if (argc > 1) {
    bsm::LOG("Starting Lobby!");
    auto lobby{std::make_unique<bsm::Lobby>()};
    init_result = lobby->init(std::stoi(argv[1]), bsm::end_point_e::LOBBY);
    app = std::move(lobby);
  } else {
    bsm::LOG("Starting Server!");
    auto server{std::make_unique<bsm::Server>()};
    init_result = server->init();
    app = std::move(server);
  }
  if (!init_result) {
    bsm::LOG(init_result.error().message);
    return EXIT_FAILURE;
  }
  app->run();
  return 0;
}
