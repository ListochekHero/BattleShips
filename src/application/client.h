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
  Client();
  auto init(end_point_e socket_type, int socket) -> std::optional<Error>;
  void run() override;

private:
  auto register_console_task() -> std::optional<Error>;
  auto console_co() -> bsm_co_handle;
  auto handle_action(const ActionContext& context) -> ActionResult override;
  auto send_input_to_server(const ActionContext& context) -> ActionResult;
  using ClientAction = std::variant<PrintMessage, Quit>;
  static auto execute_action(const PrintMessage&, const ActionContext& context)
      -> ActionResult;
  static auto execute_action(const Quit&, const ActionContext& context)
      -> ActionResult;

  ConnectionView server_view_{std::numeric_limits<std::size_t>::max()};
  ConsoleHandler console_handler_;
  AtomicQueue<std::string> console_raw_tasks_;
};

} // namespace bsm

#endif
