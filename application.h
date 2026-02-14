#ifndef APPLICATION_H
#define APPLICATION_H

#include <cstddef>
#include <sys/wait.h>

#include "data_storage.h"
#include "deferred.h"
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

struct CreateLobby {};

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
  Ev init(int parrent_socket); // init() for Lobby
  virtual void run();
  void cleanup_slot(size_t slot);

private:
  Ev emplace_socket_to_pool(std::unique_ptr<SocketHandler> socket_ptr);
  Ev add_to_socket_pool();
  Ev add_to_socket_pool(int socket_fd);
  Ev init_epoll();
  Ev init_epoll_wrapper();
  void process_events(std::vector<size_t>& event_slots);
  void process_server_socket(SocketHandler& handler);
  void process_client_socket(SocketHandler& handler);
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
  CommandStatus free_socket_handler(const CommandContext& context);
  void handle_zombie_pocesses();
  void process_new_clients(std::vector<int>& new_clients);
  void run_deferred_actions();
  void execute_action(const CreateLobby& action, const CommandContext& context);

  std::unordered_map<int64_t, LobbyProcess> lobbies;
  std::vector<size_t> avaiable_slots;
  std::vector<std::unique_ptr<DeferredAction>> deferred_actions;
  CommandStatus process_message(CommandContext& context);

  NetworkEngine net_engine_;
  LobbyManager lobby_manager_;

  using Handler = CommandStatus (Server::*)(const CommandContext& context);
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
