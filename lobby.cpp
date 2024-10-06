#include "lobby.h"

int main(int argc, char* argv[]) {
  auto& logger =
      BattleShipsMain::Logger::getInstance(std::string((argv[0])));
  logger.log("Hello!");
  for (size_t i = 0; i < argc; i++)
  {
    logger.log(argv[i]);
  }
  
  // if (argc != 2) {
  //   return 1;
  // }
  int sock_fd = std::stoi(argv[1]);
#ifdef DEBUG_LOGS
#endif
}