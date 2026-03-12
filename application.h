#ifndef APPLICATION_H
#define APPLICATION_H

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <sys/wait.h>

#include "console_routine.h"
#include "deferred_actions.h"
#include "dispatcher.h"
#include "error.h"
#include "lobby_manager.h"
#include "network_routine.h"
#include "utility.h"
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

private:
  void handle_zombie_pocesses();
  bool push_to_clients_queue(size_t slot);

  using ServerAction =
      std::variant<CreateLobby, JoinLobby, ChatMessage, GeneralAction>;
  CommandStatus handle_client_cmd(CommandContext& context);
  CommandStatus execute_action(const CreateLobby& action_type,
                               const CommandContext& context);
  std::expected<LobbyView, Error> request_lobby();
  CommandStatus execute_action(const JoinLobby& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const ChatMessage& action_type,
                               const CommandContext& context);
  CommandStatus execute_action(const GeneralAction& action_type,
                               const CommandContext& context);

  std::queue<size_t> clients_queue_;
  LobbyManager lobby_manager_;

  std::mutex m_;
  std::condition_variable cv_;
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
  using ClientAction = std::variant<PrintAble, Quit>;
  using LocalClientAction = std::variant<Quit>;
  CommandStatus execute_action(const Quit&, const CommandContext& context);
  CommandStatus execute_action(const PrintAble&, const CommandContext& context);
  void register_user_input(std::string);
  void handle_input(std::string);
  CommandStatus handle_local_cmd(ParsedCommand context);
  CommandStatus execute_local_action(const Quit&);
  ConnectionView server_view_{std::numeric_limits<std::size_t>::max()};
  ConsoleHandler console_handler_;
  std::queue<std::string> input_queue_;
  std::mutex m_;
  std::condition_variable cv_;
};

} // namespace bsm
#endif
