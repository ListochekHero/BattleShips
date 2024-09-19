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
  SocketHandler(int port);
  ~SocketHandler();
  int get_fd() const;
  void set_fd(int sock_fd);
  void accept_connection();
  void update(int sock_fd) override;
  void set_connection_callback(ConnectionCallback callback);
 private:
  int sock_fd;
  ConnectionCallback connection_callback;
  void make_non_blocking();
};

class SocketIOHandler : private SocketHandler, Observer {
  public:
  void read_from_socket();
  private:
    std::string permanent_buffer;
};
class EpollHandler : public ObservableSu8ject {
 public:
  EpollHandler();
  ~EpollHandler();
  void add_socket(int sock_fd);
  void wait_for_events(int max_events);
  void attach(int sock_fd, Observer *observer) override;
  void detach(Observer *observer) override;

 private:
  int epollfd;
  std::map<int, Observer *> observers;

  void notify(int sock_fd) override;
};

}  // namespace BattleShipsMain

#endif