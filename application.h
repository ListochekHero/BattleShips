#ifndef APPLICATION_H
#define APPLICATION_H

#include <sys/wait.h>

#include <list>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <atomic>
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
  void run();

 private:
  EpollHandler epoll_handler;
  std::vector<SocketHandler> sockets;
  std::unordered_map<uint64_t, SocketHandler> lobbies;
  // std::unique_ptr<std::list<Lobbies>> lobbies;

  // std::map<const int, std::unique_ptr<Observer>> observers;
  std::expected<void, std::string> handle_client_cmd(
      const SocketHandler& client, std::string_view command);
  void handle_client_request_for_lobby(int client_fd);
  void handle_zombie_pocesses();
  void close_all_sockets_but_one(int sock_fd);
  std::expected<void, std::string> create_lobby(const SocketHandler& client);
};
class Client : public Application {
 public:
 private:
};
}  // namespace bsm
#endif
