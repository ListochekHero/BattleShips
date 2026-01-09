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

 protected:
  EpollHandler epoll_handler;
  std::vector<std::unique_ptr<SocketHandler>> sockets;

 private:
};
class Server : public Application {
 public:
  Server() = default;
  std::expected<void, std::string> init();  // init() for Server
  std::expected<void, std::string> init(
      SocketHandler& parrent_socket);  // init() for Lobby
  virtual void run();

 private:
  void process_events(std::vector<SocketHandler*>& events);
  void process_server_socket(SocketHandler* handler);
  void process_client_socket(SocketHandler* handler);
  std::expected<void, std::string> handle_client_cmd(SocketHandler& client,
                                                     std::string_view command);
  std::expected<void, std::string> create_lobby(SocketHandler& client);
  std::expected<void, std::string> write_to_child(int64_t child_id,
                                                  std::string_view command,
                                                  std::string_view message);
  void erase_socket_handler(SocketHandler& client);
  void handle_zombie_pocesses();

  std::unordered_map<int64_t, SocketHandler> lobbies;
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
  std::expected<void, std::string> handle_client_cmd(
      const SocketHandler& client, std::string_view command);
  std::expected<void, std::string> accept_socket(const SocketHandler&);
  std::expected<void, std::string> send_connection_code(const SocketHandler&);
  std::expected<void, std::string> erase_socket_handler(
      const SocketHandler& client);
};
}  // namespace bsm
#endif
