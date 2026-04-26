#ifndef APP_CLIENT_H
#define APP_CLIENT_H

// IWYU pragma: begin_exports
#include "application/application.h"
#include "core/atomic_queue.h"
#include "core/dispatcher.h"
#include "io/console_routine.h"
#include "protocol/coroutine_promise.h"
#include "protocol/network_defs.h"
#include "protocol/network_types.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <atomic>
#include <cstddef>
#include <limits>
#include <string>
#include <variant>
// IWYU pragma: end_export

namespace bsm {

class Client : public Application {
public:
  auto console_co() -> bsm_co_handle;
  Client();
  auto v_init(end_point_e socket_type, int parrent_socket) -> Ev;
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
};

} // namespace bsm

#endif
