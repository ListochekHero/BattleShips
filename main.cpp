#include "main.h"

int main(int argc, char* argv[]) {
  char* program_name = strrchr(argv[0], '/');
  program_name++;
  auto& logger =
      BattleShipsMain::Logger::getInstance(std::string(program_name));
#ifdef DEBUG_LOGS
#endif
  logger.log("DEBUG Enabled!");
  BattleShipsMain::Logger::getInstance().log("TEST MACRO");

  LOG("TEST MACRO");
  std::string message("message");
  LOG(message);
  std::unique_ptr<BattleShipsMain::Server> server =
      std::make_unique<BattleShipsMain::Server>(8000);
  logger.log("starting server!");
  server->run();
}