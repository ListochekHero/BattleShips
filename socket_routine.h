#ifndef SOCKET_HANDLING_H
#define SOCKET_HANDLING_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <vector>

#include "interfaces.h"

#define BACKLOG 10
#define BUFF_SIZE 1024
namespace BattleShipsMain {

class SocketHandler : public Observer {
 public:
  using ConnectionCallback = std::function<void(int)>;
  SocketHandler() = default;
  SocketHandler(int port);
  virtual ~SocketHandler();
  int get_fd() const;
  void set_fd(int sock_fd);
  void accept_connection();
  void update(int sock_fd) override;
  void set_connection_callback(ConnectionCallback callback);
  ConnectionCallback get_connection_callback();

 private:
  int sock_fd = 0;
  ConnectionCallback connection_callback;
  void make_non_blocking();
};

class SocketIOHandler : public SocketHandler {
 public:
  SocketIOHandler(int sock_fd);
  ~SocketIOHandler();
  void update(int sock_fd) override;
  void read_from_socket();
  std::string get_client_data();

 private:
  std::string client_data;
};
class EpollHandler {
 public:
  EpollHandler();
  ~EpollHandler();
  void add_socket(int sock_fd);
  std::vector<int> wait_for_events(int max_events);

 private:
  int epollfd = 0;
};

}  // namespace BattleShipsMain

#endif