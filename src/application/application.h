#ifndef APPLICATION_H
#define APPLICATION_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <sys/wait.h>
#include <variant>

#include "core/dispatcher.h"
#include "core/scheduler.h"
#include "io/console_routine.h"
#include "net/network_routine.h"
#include "services/lobby_manager.h"
#include "utility/error.h"
#include "utility/utility.h"

namespace bsm {

class Application {
public:
  virtual void run() = 0;
  virtual ~Application();
  Application();
  std::expected<ConnectionView, Error> init(end_point_e socket_type,
                                            int parrent_socket);
  virtual CommandStatus handle_client_cmd(const CommandContext& context) = 0;
  bsm_co_handle network_co();

protected:
  NetworkEngine& network_engine();
  Dispatcher& dispatcher();
  Scheduler& scheduler();
  AtomicQueue<task_tag_e>& available_task_tags();

private:
  NetworkEngine network_engine_;
  AtomicQueue<size_t> network_raw_tasks_;
  Dispatcher dispatcher_;
  Scheduler scheduler_;
  AtomicQueue<task_tag_e> available_task_tags_;
};

class Server : public Application {
public:
  Server() = default;
  Ev init();
  void run() override;
  Ev init(end_point_e socket_type, int parrent_socket);

private:
  void handle_zombie_pocesses();

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

  LobbyManager lobby_manager_;
};

class Lobby : public Application {
public:
  Lobby() = default;
  Ev init(int parrent_socket, end_point_e socket_type);
  Ev init(end_point_e socket_type, int parrent_socket);
  void run() override;
  std::atomic_size_t pending_clients_counter_{0};

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
  bsm_co_handle console_co();
  Client();
  Ev init(end_point_e socket_type, int parrent_socket);
  Ev init(end_point_e socket_type);
  void run();
  CommandStatus handle_client_cmd(const CommandContext& context);
  CommandStatus handle_input(const CommandContext& context);
  std::atomic_size_t pending_clients_counter_{0};

private:
  using ClientAction = std::variant<PrintAble, Quit>;
  using LocalClientAction = std::variant<Quit>;
  CommandStatus execute_action(const Quit&, const CommandContext& context);
  CommandStatus execute_action(const PrintAble&, const CommandContext& context);
  void register_user_input(std::string);
  CommandStatus handle_local_cmd(ParsedCommand context);
  CommandStatus execute_local_action(const Quit&);
  ConnectionView server_view_{std::numeric_limits<std::size_t>::max()};
  ConsoleHandler console_handler_;
  AtomicQueue<std::string> console_raw_tasks_;

  std::mutex m_;
  std::condition_variable cv_;
};

} // namespace bsm
#endif
