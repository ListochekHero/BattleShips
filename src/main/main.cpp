#include "main.h"

#include "application/application.h"
#include "utility/config.h"
#include "utility/logger.h"

#include <cstdlib>
#include <iostream>
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
    init_result = lobby->init(bsm::end_point_e::TO_PARENT, std::stoi(argv[1]));
    app = std::move(lobby);
  } else {
    bsm::LOG("Starting Server!");
    auto server{std::make_unique<bsm::Server>()};
    init_result = server->init(bsm::end_point_e::LISTENER, 0);
    app = std::move(server);
  }
  if (!init_result) {
    bsm::LOG(init_result.error().full_report());
    return EXIT_FAILURE;
  }
  app->run();
  return EXIT_SUCCESS;
}
