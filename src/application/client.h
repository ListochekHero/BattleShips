#ifndef APP_CLIENT_H
#define APP_CLIENT_H

#include "application/application.h"
#include "io/console_routine.h"

namespace bsm {

class Client : public Application {
public:
  auto console_co() -> bsm_co_handle;
  Client();
  auto init(end_point_e socket_type, int parrent_socket) -> Ev;
  auto init(end_point_e socket_type) -> Ev;
  void run() override;
  auto handle_client_cmd(const CommandContext& context)
      -> CommandStatus override;
  auto handle_input(const CommandContext& context) -> CommandStatus;
  std::atomic_size_t pending_clients_counter_{0};

private:
  using ClientAction = std::variant<PrintAble, Quit>;
  using LocalClientAction = std::variant<Quit>;
  static auto execute_action(const Quit&, const CommandContext& context)
      -> CommandStatus;
  static auto execute_action(const PrintAble&, const CommandContext& context)
      -> CommandStatus;

  ConnectionView server_view_{std::numeric_limits<std::size_t>::max()};
  ConsoleHandler console_handler_;
  AtomicQueue<std::string> console_raw_tasks_;

  std::mutex m_;
  std::condition_variable cv_;
};

} // namespace bsm

#endif
