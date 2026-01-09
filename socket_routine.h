#ifndef SOCKET_HANDLING_H
#define SOCKET_HANDLING_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <expected>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "logger.h"

// #include "interfaces.h"

#define BACKLOG 10
#define BUFF_SIZE 1024
namespace bsm {

enum class socket_type_e { UNKNOWN, SERVER, IPC, CLIENT, SPECTATOR };

class SocketHandler {
 public:
  SocketHandler() = default;
  SocketHandler(int socket_fd);
  ~SocketHandler();
  SocketHandler(const SocketHandler&) = delete;
  SocketHandler(SocketHandler&&);
  SocketHandler& operator=(const SocketHandler&) = delete;
  SocketHandler& operator=(SocketHandler&&);
  std::expected<void, std::string> setup_listenter(int port);
  const int get_socket() const;
  socket_type_e get_socket_type() const;
  void set_socket_type(socket_type_e socket_type);
  std::expected<std::vector<std::unique_ptr<SocketHandler>>, std::string>
  accept_connections() const;
  std::expected<std::string, std::string> read_user_input() const;
  std::expected<void, std::string> write_to_user(
      std::string_view string_to_send) const;
  std::expected<void, std::string> remove_cloexec();

 private:
  int socket_fd{-1};
  socket_type_e socket_type_v{-1};

  void swap(SocketHandler& left_sh, SocketHandler& r_sh);
  void close_socket();
};

class EpollHandler {
 public:
  EpollHandler() = default;
  ~EpollHandler();
  std::expected<void, std::string> init();
  std::expected<void, std::string> add_socket(
      SocketHandler* const socket_handler);
  std::expected<void, std::string> remove_socket(
      const SocketHandler* const socket_handler);
  std::expected<std::vector<SocketHandler*>, std::string> wait_for_events(
      size_t max_events) const;

 private:
  int epollfd = 0;
};

}  // namespace bsm

#endif
