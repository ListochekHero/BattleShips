#ifndef APPLICATION_H
#define APPLICATION_H

#include <sys/wait.h>

#include "config.h"
#include "data_storage.h"
#include "logger.h"
#include "socket_routine.h"
#include "utility.h"
#include <memory>
#include <optional>
#include <unordered_map>

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
  Ev init();                               // init() for Server
  Ev init(SocketHandler&& parrent_socket); // init() for Lobby
  virtual void run();

private:
  Ev init_epoll();
  Ev init_epoll_wrapper();
  void process_events(std::vector<SocketHandler*>& events);
  void process_server_socket(SocketHandler* handler);
  void process_client_socket(SocketHandler* handler);
  CommandStatus handle_client_cmd(SocketHandler& client,
                                  const ReadResult& message);
  std::expected<LobbyProcess, Error> spawn_lobby_process();
  CommandStatus create_lobby(SocketHandler& client, const ReadResult& message);
  Ev init_child(const SocketHandler& child_socket, int64_t child_id,
                SocketHandler& client);
  CommandStatus accept_socket(SocketHandler& parrent,
                              const ReadResult& message);
  CommandStatus send_connection_code(SocketHandler& parrent,
                                     const ReadResult& message);
  CommandStatus join_lobby(SocketHandler& client, const ReadResult& message);
  CommandStatus general_command(SocketHandler& client,
                                const ReadResult& message);
  std::expected<LobbyProcess*, Error> found_lobby(int64_t child_id);
  CommandStatus chat_message(SocketHandler& client, const ReadResult& message);

  Ev send_command_error_reply(const SocketHandler& client, Error& error);
  CommandStatus erase_socket_handler(SocketHandler& client,
                                     const ReadResult& message);
  void handle_zombie_pocesses();
  std::unordered_map<int64_t, LobbyProcess> lobbies;
  CommandStatus process_message(SocketHandler* handler,
                                const ReadResult& message);

  using Handler = CommandStatus (Server::*)(SocketHandler&, const ReadResult&);
  struct Command {
    Handler handler;
    struct Match {
      std::vector<std::string_view> text_aliases;
      std::optional<message_type_e> msg_type;
    } match;
  };
  static const std::array<Command, 6> commands;
  bool match_cmd(const Command& cmd, const ReadResult& msg);
};
class Client : public Application {
public:
private:
};

} // namespace bsm
#endif
