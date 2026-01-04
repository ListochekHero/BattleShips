#ifndef SOCKET_HANDLING_H
#define SOCKET_HANDLING_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <algorithm>
#include <expected>
#include <functional>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

// #include "interfaces.h"

#define BACKLOG 10
#define BUFF_SIZE 1024
namespace bsm {

class SocketHandler {
 public:
  SocketHandler() = default;
  SocketHandler(int socket_fd);
  virtual ~SocketHandler();
  SocketHandler(const SocketHandler&) = delete;
  SocketHandler(SocketHandler&&);
  SocketHandler& operator=(SocketHandler&&);
  std::expected<void, std::string> setup_listenter(int port);
  int get_socket() const;
  std::expected<std::vector<SocketHandler>, std::string> accept_connections();
  std::expected<std::string, std::string> read_user_input();
  bool is_listening() const;

 private:
  bool listening_socket{false};
  int socket_fd{-1};
  void swap(SocketHandler& left_sh, SocketHandler& r_sh);
  void close_socket();
};

class EpollHandler {
 public:
  EpollHandler() = default;
  ~EpollHandler();
  std::expected<void, std::string> init();
  std::expected<void, std::string> add_socket(SocketHandler& socket_handler);
  std::expected<std::vector<SocketHandler*>, std::string> wait_for_events(
      int max_events);

 private:
  int epollfd = 0;
};

}  // namespace bsm

#endif
