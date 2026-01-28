#ifndef UTILITY_H
#define UTILITY_H

#include <expected>
#include <filesystem>
#include <system_error>

namespace bsm {
enum class internal_error_e {
  INVALID_ARGS,
  NOT_FOUND,
  PERMISSION_DENIED,
  ALREADY_EXIST,
  INTERNAL,
  SYSTEM
};
enum class user_error_e { GENERIC, CANT_CREATE_LOBBY, CANT_JOIN_LOBBY };

struct Error {
  internal_error_e error_code;
  std::string message;
  user_error_e user_code{0};
};

inline Error make_error(internal_error_e code, std::string message) {
  return {.error_code = code, .message = std::move(message)};
}

std::string c_error_string();
inline Error make_error_c(internal_error_e code, std::string message) {
  return {.error_code = code,
          .message = std::format("{}: {}", message, c_error_string())};
}

std::string_view user_message(user_error_e user);
bool is_file_exist(const std::string& filename);
int64_t generate_conn_code();
std::expected<std::string, Error> parse(const std::string& message);
std::expected<std::string, Error> generate_name();

using er_e = internal_error_e;
using us_e = user_error_e;
using Ev = std::expected<void, Error>;
} // namespace bsm

#endif
