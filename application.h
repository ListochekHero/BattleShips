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
  friend struct Command;
  Server() = default;
  Ev init();                                // init() for Server
  Ev init(SocketHandler&& parrent_socket);  // init() for Lobby
  virtual void run();

 private:
  Ev init_epoll();
  void process_events(std::vector<SocketHandler*>& events);
  void process_server_socket(SocketHandler* handler);
  void process_client_socket(SocketHandler* handler);
  void handle_client_cmd(SocketHandler& client, const std::string& command);
  Ev create_lobby(SocketHandler& client);
  Ev write_to_child(SocketHandler* child_ipc, const std::string& command,
                    const std::string& message);
  Ev accept_socket(SocketHandler& parrent);
  Ev send_connection_code(SocketHandler& parrent);
  Ev general_command(SocketHandler& client, const std::string& command);
  std::expected<SocketHandler*, std::string> found_lobby(int64_t child_id);

  Ev erase_socket_handler(SocketHandler& client);
  void handle_zombie_pocesses();

  std::unordered_map<int64_t, SocketHandler> lobbies;

  using Handler = Ev (Server::*)(SocketHandler&);
  struct Command {
    std::string_view command;
    Handler handler;
  };
  static const std::array<Command, 4> commands;
};
class Client : public Application {
 public:
 private:
};

}  // namespace bsm
#endif
