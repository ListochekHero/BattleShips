#ifndef APP_CLIENT_H
#define APP_CLIENT_H

#include "application/application.h"
#include "io/console_routine.h"

namespace bsm {

class Client : public Application {
public:
  bsm_co_handle console_co();
  Client();
  Ev init(end_point_e socket_type, int parrent_socket);
  Ev init(end_point_e socket_type);
  void run() override;
  CommandStatus handle_client_cmd(const CommandContext& context) override;
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
