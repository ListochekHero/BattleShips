#include "main.h"

int main(int argc, char *argv[]){
    auto& logger = BattleShipsMain::Logger::getInstance(argv[0]);
    logger.log("Hello!");
    std::unique_ptr<BattleShipsMain::Server> server = std::make_unique< BattleShipsMain::Server>(8000);
    server->run();
}