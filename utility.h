#ifndef UTILITY_H
#define UTILITY_H

#include <expected>
#include <format>
#include <optional>
#include <string>

namespace bsm {

class ConnectionView;
struct ReadResult;
enum class end_point_e : uint8_t;

enum class user_error_e {
  GENERIC,
  CANT_CREATE_LOBBY,
  CANT_JOIN_LOBBY,
  UNKNOWN_COMMAND
};
using us_e = user_error_e;

enum class command_status_e { CONTINUE, TERMINATE };
using cmd_se = command_status_e;

struct Error {
  std::string message;
};
using Ev = std::expected<void, Error>;

struct CommandStatus {
  command_status_e command_status_v;
  std::optional<us_e> user_code{us_e::GENERIC};
  std::optional<Error> error{std::nullopt};
};

struct CommandContext {
  ConnectionView& client;
  ReadResult& message;
  end_point_e peer;
};

std::string c_error_string();
inline Error make_error_c(std::string message) {
  return {.message = std::format("{}: {}", message, c_error_string())};
}

std::string_view user_message(user_error_e user);
bool is_file_exist(const std::string& filename);
int64_t generate_conn_code();
std::expected<std::string, Error> parse(const std::string& message);
std::expected<std::string, Error> generate_name();

template <typename T, typename E> void drop_result(std::expected<T, E>&&) {}
} // namespace bsm

#endif
