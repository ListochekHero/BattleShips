#ifndef UTILITY_H
#define UTILITY_H

#include "error.h"
#include "logger.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <variant>

namespace bsm {

class ConnectionView;
struct ReadResult;
enum class end_point_e : uint8_t;

enum class command_status_e { CONTINUE, TERMINATE };
using cmd_se = command_status_e;

struct CommandStatus {
  command_status_e command_status_v;
  std::optional<Error> error{std::nullopt};
  std::optional<us_e> user_code{us_e::GENERIC};
};

struct CommandContext {
  ConnectionView& client_view;
  ReadResult& message;
  end_point_e peer;
};

struct DeliveryReport {
  size_t delivered{0};
  size_t failed{0};
};

bool is_file_exist(const std::string& filename);
int64_t generate_conn_code();
std::expected<std::string, Error> parse(const std::string& message);
std::expected<std::string, Error> generate_name();
template <typename T, typename E> void drop_result(std::expected<T, E>&&) {}
template <typename T, typename E>
void success_or_terminate(std::expected<T, E>&& r) {
  if (!r) {
    LOG(r.error().full_report());
    LOG("Critical error occured, terminating...");
    std::terminate();
  }
}

template <typename T, typename Variant> struct is_alternative_of;

template <typename T, typename... Args>
struct is_alternative_of<T, std::variant<Args...>>
    : std::disjunction<std::is_same<T, Args>...> {};

template <typename T, typename Variant>
inline constexpr bool is_alternative_of_v =
    is_alternative_of<T, Variant>::value;

template <typename TargetVariant, typename SourceVariant>
std::expected<TargetVariant, Error> filter_variant(SourceVariant&& source) {
  return std::visit(
      [](auto&& arg) -> std::expected<TargetVariant, Error> {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (is_alternative_of_v<T, TargetVariant>) {
          return TargetVariant(std::forward<decltype(arg)>(arg));
        } else {
          return std::unexpected(
              Error{{"Command not allowed in this context"}});
        }
      },
      std::forward<SourceVariant>(source));
}

} // namespace bsm

#endif
