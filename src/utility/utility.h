#ifndef UTILITY_H
#define UTILITY_H
// IWYU pragma: no_include <vector>

#include "protocol/message/message_types.h"
#include "utility/error.h"
#include "utility/logger.h"

#include <concepts>
#include <cstdint>
#include <exception>
#include <expected>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace bsm {

class ConnectionView;
struct ReceivedMessage;

enum class ActionStatus : uint8_t { CONTINUE, TERMINATE };
enum class ConnectionStatus : uint8_t { KEEP, RELEASE };

struct ActionResult {
  ActionStatus action_status{ActionStatus::CONTINUE};
  ConnectionStatus conn_status{ConnectionStatus::KEEP};
  std::optional<Error> error{std::nullopt};
  std::optional<user_error_e> user_code{std::nullopt};
};

struct ActionContext {
  const ConnectionView& pending_view;
  const ReceivedMessage& received_message;
};

auto is_file_exist(const std::string& filename) -> bool;
auto generate_conn_code() -> int64_t;
auto parse(const std::string& message) -> std::expected<std::string, Error>;
auto generate_name() -> std::expected<std::string, Error>;
template <typename T, typename E>
void drop_result(std::expected<T, E>&& /*unused*/) {}

inline void plain_terminate() {
  LOG("Critical error occured, terminating...");
  std::terminate();
}
inline void success_or_terminate(const std::optional<Error>& error) {
  if (error) {
    LOG(error->full_report());
    plain_terminate();
  }
}
template <typename F, typename U>
inline void success_or_terminate(std::expected<F, U>&& result) {
  if (result) {
    LOG(result.error().full_report());
    plain_terminate();
  }
}

template <typename T, typename Variant>
concept AlternativeOf = []<typename... Args>(std::variant<Args...>*) -> auto {
  return (std::same_as<T, Args> || ...);
}(static_cast<Variant*>(nullptr));

template <typename TargetVariant, typename SourceVariant>
std::expected<TargetVariant, Error> filter_variant(SourceVariant&& source) {
  return std::visit(
      [](auto&& arg) -> std::expected<TargetVariant, Error> {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (AlternativeOf<T, TargetVariant>) {
          return TargetVariant(std::forward<decltype(arg)>(arg));
        } else {
          return std::unexpected(Error{
              .backtrace = {"No such command can be found as supported"}});
        }
      },
      std::forward<SourceVariant>(source));
}

} // namespace bsm

#endif
