#ifndef UTILITY_H
#define UTILITY_H

#include <expected>
#include <format>
#include <optional>
#include <string>

namespace bsm {

class ConnectionView;
struct ReadResult;

enum class PeerKind { CLIENT, LOBBY };

enum class internal_error_e {
  GENERIC,
  INVALID_ARGS,
  NOT_FOUND,
  PERMISSION_DENIED,
  ALREADY_EXIST,
  INTERNAL,
  SYSTEM
};
enum class user_error_e {
  GENERIC,
  CANT_CREATE_LOBBY,
  CANT_JOIN_LOBBY,
  UNKNOWN_COMMAND
};
enum class command_status_e { CONTINUE, TERMINATE };

struct Error {
  std::string message;
  user_error_e user_code{0};
};

struct CommandStatus {
  command_status_e command_status_v;
  std::optional<Error> error{std::nullopt};
};

struct CommandContext {
  ConnectionView& client;
  ReadResult& message;
  PeerKind peer;
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

using er_e = internal_error_e;
using us_e = user_error_e;
using cmd_se = command_status_e;
using Ev = std::expected<void, Error>;
} // namespace bsm

#endif
