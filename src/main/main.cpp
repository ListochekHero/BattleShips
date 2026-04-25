#include "main.h"

#include "application/application.h"
#include "application/lobby.h"
#include "application/server.h"
#include "protocol/network_defs.h"
#include "utility/config.h"
#include "utility/error.h"
#include "utility/logger.h"
#include <cstring>
#include <expected>
#include <string>
#include <utility>

#include <cstdlib>
#include <iostream>
#include <memory>

auto main(int argc, char* argv[]) -> int {
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
    init_result =
        lobby->v_init(bsm::end_point_e::TO_PARENT, std::stoi(argv[1]));
    app = std::move(lobby);
  } else {
    bsm::LOG("Starting Server!");
    auto server{std::make_unique<bsm::Server>()};
    init_result = server->v_init(bsm::end_point_e::LISTENER, 0);
    app = std::move(server);
  }
  if (!init_result) {
    bsm::LOG(init_result.error().full_report());
    return EXIT_FAILURE;
  }
  app->run();
  return EXIT_SUCCESS;
}
