#ifndef SERVER_H
#define SERVER_H

#include <sys/wait.h>

#include "socket_handling.h"

#define MAX_EVENTS 10

namespace BattleShipsMain {
class Server {
  Server(int port);
  void run();

 private:
  SocketHandler server_socket;
  EpollHandler epoll_handler;

  void handle_connection(int clientfd);
  void handle_zombie_pocesses();
};
}  // namespace BattleShipsMain
#endif