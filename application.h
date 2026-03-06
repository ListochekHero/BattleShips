#ifndef APPLICATION_H
#define APPLICATION_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <sys/wait.h>

#include "deferred_actions.h"
#include "dispatcher.h"
#include "error.h"
#include "lobby_manager.h"
#include "network_routine.h"
#include "utility.h"
#include <type_traits>
#include <unordered_map>
#include <variant>

#define MAX_EVENTS 10

namespace bsm {

class Application {
public:
  virtual void run() = 0;
  virtual ~Application() = default;

protected:
  Dispatcher& dispatcher();
  NetworkEngine& net_engine();

private:
  NetworkEngine net_engine_;
  Dispatcher dispatcher_;
};

class Server : public Application {
public:
  Server() = default;
  Ev init();
  void run() override;
  void cleanup_slot(size_t slot);

private:
  CommandStatus accept_socket_from_parent(const CommandContext& context);
  CommandStatus send_connection_code(const CommandContext& context);
  CommandStatus join_lobby(const CommandContext& context);
  CommandStatus general_command(const CommandContext& context);
  std::expected<LobbyProcess*, Error> found_lobby(int64_t child_id);
  CommandStatus chat_message(const CommandContext& context);
  Ev send_error_reply(const ConnectionView& client, user_error_e error);
  void handle_zombie_pocesses();

  using ServerAction = std::variant<CreateLobby, JoinLobby, Quit, ChatMessage,
                                    NotAllowed, GeneralAction>;
  CommandStatus handle_client_cmd(CommandContext& context);
  CommandStatus execute_action(const CreateLobby& action_type,
                               const CommandContext& context);
  std::expected<LobbyView, Error> request_lobby();
  CommandStatus execute_action(const JoinLobby& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const Quit& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const AcceptSocket& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const LobbyIdSetter& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const ChatMessage& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const NotAllowed& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const GeneralAction& action_type,
                               const CommandContext& context);
  std::unordered_map<int64_t, LobbyProcess> lobbies;
  CommandStatus process_message(CommandContext& context);

  LobbyManager lobby_manager_;
};

class Lobby : public Application {
public:
  Lobby() = default;
  Ev init(int parrent_socket, end_point_e socket_type);
  void run() override;

private:
  using LobbyAction = std::variant<AcceptSocket, LobbyIdSetter>;
  CommandStatus handle_client_cmd(CommandContext& context);
  CommandStatus execute_action(const AcceptSocket& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const LobbyIdSetter& action_type,
                               const CommandContext& context);
  int64_t lobby_id_{0};
  ConnectionView parent_view_{std::numeric_limits<std::size_t>::max()};
};

class Client : public Application {
public:
  Ev init(end_point_e socket_type);
  void run();
  CommandStatus handle_client_cmd(CommandContext& context);

private:
  using ClientAction = std::variant<Quit>;
  CommandStatus execute_action(const Quit&, const CommandContext& context);
  ConnectionView server_view_{std::numeric_limits<std::size_t>::max()};
};

} // namespace bsm
#endif
