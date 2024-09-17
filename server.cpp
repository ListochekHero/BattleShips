#include "server.h"

namespace BattleShipsMain {
class Server {
 public:
  Server(int port) : server_socket(port), epoll_handler() {
    epoll_handler.add_socket(server_socket.get_fd());
  }

 private:
  SocketHandler server_socket;
  EpollHandler epoll_handler;
};
}  // namespace BattleShipsMain