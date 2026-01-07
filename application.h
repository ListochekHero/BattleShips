#ifndef APPLICATION_H
#define APPLICATION_H

#include <sys/wait.h>

#include <atomic>
#include <list>
#include <memory>
#include <ranges>
#include <unordered_map>
#include "config.h"
#include "data_storage.h"
#include "logger.h"
#include "socket_routine.h"

#define MAX_EVENTS 10

namespace bsm {
class Application {
 public:
  virtual void run() = 0;
  virtual ~Application() = default;

 private:
};
class Server : public Application {
 public:
  Server() = default;
  std::expected<void, std::string> init();
  virtual void run();

 private:
  EpollHandler epoll_handler;
  std::vector<std::unique_ptr<SocketHandler>> sockets;
  std::unordered_map<int64_t, SocketHandler> lobbies;
  // std::unique_ptr<std::list<Lobbies>> lobbies;

  // std::map<const int, std::unique_ptr<Observer>> observers;
  std::expected<void, std::string> handle_client_cmd(
      const SocketHandler& client, std::string_view command);
  std::expected<void, std::string> create_lobby(const SocketHandler& client);
  std::expected<void, std::string> erase_socket_handler(
      const SocketHandler& client);
  void handle_zombie_pocesses();
};
class Client : public Application {
 public:
 private:
};
class Lobby : public Application {
 public:
  std::expected<void, std::string> init(SocketHandler&);
  virtual void run() override;

 private:
  EpollHandler epoll_handler;
  std::vector<std::unique_ptr<SocketHandler>> sockets;
  std::expected<void, std::string> handle_client_cmd(
      const SocketHandler& client, std::string_view command);
  std::expected<void, std::string> accept_socket(const SocketHandler&);
  std::expected<void, std::string> send_connection_code(const SocketHandler&);
  std::expected<void, std::string> erase_socket_handler(
      const SocketHandler& client);
};
}  // namespace bsm
#endif
