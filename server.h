#ifndef SERVER_H
#define SERVER_H

#include <sys/wait.h>

#include <memory>

#include "socket_routine.h"

#define MAX_EVENTS 10

namespace BattleShipsMain {
class Server : public Application {
  Server(int port);
  void run();

 private:
  std::unique_ptr<SocketHandler> server_socket;
  std::unique_ptr<EpollHandler> epoll_handler;

  void handle_connection(int clientfd);
  void handle_zombie_pocesses();
};
}  // namespace BattleShipsMain
#endif