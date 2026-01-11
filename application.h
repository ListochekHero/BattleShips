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
  //friend struct Command;
  Server() = default;
  std::expected<void, std::string> init();  // init() for Server
  std::expected<void, std::string> init(
      SocketHandler& parrent_socket);  // init() for Lobby
  virtual void run();

 public:
  void process_events(std::vector<SocketHandler*>& events);
  void process_server_socket(SocketHandler* handler);
  void process_client_socket(SocketHandler* handler);
  std::expected<void, std::string> handle_client_cmd(
      SocketHandler& client, const std::string& command);
  std::expected<void, std::string> create_lobby(SocketHandler& client);
  std::expected<void, std::string> write_to_child(SocketHandler* child_ipc,
                                                  const std::string& command,
                                                  const std::string& message);
  std::expected<void, std::string> accept_socket(const SocketHandler& parrent);
  std::expected<void, std::string> send_connection_code(
      const SocketHandler& parrent);
  std::expected<void, std::string> general_command(SocketHandler& client,
                                                   const std::string& command);
  std::expected<SocketHandler*, std::string> found_lobby(int64_t child_id);

  void erase_socket_handler(SocketHandler& client);
  void handle_zombie_pocesses();

  std::unordered_map<int64_t, SocketHandler> lobbies;
};
class Client : public Application {
 public:
 private:
};

using Handler = std::expected<void, std::string> (Server::*)(SocketHandler&);

struct Command {
  std::string command;
  Handler handler;
};

static Command commands[] = {{"\\create", &Server::create_lobby},
                             {"close", &Server::erase_socket_handler},
                             {
                                 "\\socket", &Server::accept_socket
                             },
                             {
                                 "\\conn_code",
                             },
                             {
                                 "test",
                             }};

}  // namespace bsm
#endif
