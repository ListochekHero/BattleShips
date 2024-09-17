#ifndef SOCKET_HANDLING_H
#define SOCKET_HANDLING_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <iostream>
#include <vector>

#define BACKLOG 10

namespace BattleShipsMain {

class SocketHandler {
 public:
  SocketHandler(int port);
  ~SocketHandler();
  int get_fd() const;

 private:
  int sock_fd;

  void make_non_blocking();
};
class EpollHandler {
 public:
  EpollHandler();
  ~EpollHandler();
  void add_socket(int sock_fd);
  std::vector<int> wait_for_events(int max_events);
  int epoll_fd;
};

}  // namespace BattleShipsMain

#endif