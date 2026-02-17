#ifndef APPLICATION_H
#define APPLICATION_H

#include <cstddef>
#include <sys/wait.h>

#include "data_storage.h"
#include "deferred_actions.h"
#include "lobby_manager.h"
#include "network_routine.h"
#include "socket_routine.h"
#include "utility.h"
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#define MAX_EVENTS 10

namespace bsm {

struct CreateLobby;

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
  Ev init();                   // init() for Server
  Ev init(int parrent_socket, end_point_type_e socket_type); // init() for Lobby
  virtual void run();
  void cleanup_slot(size_t slot);

private:
  CommandStatus handle_client_cmd(CommandContext& context);
  std::expected<LobbyProcess, Error> spawn_lobby_process();
  CommandStatus create_lobby(const CommandContext& context);
  Ev init_child(const SocketHandler& child_socket, int64_t child_id,
                SocketHandler& client);
  CommandStatus accept_socket_from_parent(const CommandContext& context);
  CommandStatus send_connection_code(const CommandContext& context);
  CommandStatus join_lobby(const CommandContext& context);
  CommandStatus general_command(const CommandContext& context);
  std::expected<LobbyProcess*, Error> found_lobby(int64_t child_id);
  CommandStatus chat_message(const CommandContext& context);

  Ev send_error_reply(const SocketHandler& client, Error& error);
  void handle_zombie_pocesses();
  void execute_action(const CreateLobby& action, const CommandContext& context);

  std::unordered_map<int64_t, LobbyProcess> lobbies;
  CommandStatus process_message(CommandContext& context);

  NetworkEngine net_engine_;
  LobbyManager lobby_manager_;
};

class Client : public Application {
public:
private:
};

} // namespace bsm
#endif
