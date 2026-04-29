#ifndef UTILITY_H
#define UTILITY_H
// IWYU pragma: no_include <vector>

#include "utility/error.h"
#include "utility/logger.h"

#include <concepts>
#include <cstddef>
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
struct ReadResult;

enum class command_status_e : uint8_t { CONTINUE, TERMINATE };
using cmd_se = command_status_e;

struct CommandStatus {
  command_status_e command_status_v;
  std::optional<Error> error{std::nullopt};
  std::optional<us_e> user_code{us_e::GENERIC};
};

struct CommandContext {
  const ConnectionView& client_view;
  const ReadResult& message;
};

auto is_file_exist(const std::string& filename) -> bool;
auto generate_conn_code() -> int64_t;
auto parse(const std::string& message) -> std::expected<std::string, Error>;
auto generate_name() -> std::expected<std::string, Error>;
template <typename T, typename E>
void drop_result(std::expected<T, E>&& /*unused*/) {}
template <typename T, typename E>
void success_or_terminate(std::expected<T, E>&& r) {
  if (!r) {
    LOG(r.error().full_report());
    LOG("Critical error occured, terminating...");
    std::terminate();
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
