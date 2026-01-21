#ifndef UTILITY_H
#define UTILITY_H

#include <expected>
#include <filesystem>
#include <system_error>

namespace bsm {
enum class error_code_e {
  INVALID_ARGS,
  NOT_FOUND,
  PERMISSION_DENIED,
  ALREADY_EXIST,
  INTERNAL,
  SYSTEM
};
struct Error {
  error_code_e code;
  std::string message;
};

inline Error make_error(error_code_e code, std::string message) {
  return {code, std::move(message)};
}
inline Error make_error_c(error_code_e code, std::string message) {
  return {code, std::move(std::format("{}: {}", message, c_error_string()))};
}

std::string_view user_message(error_code_e code);
bool is_file_exist(const std::string& filename);
std::string c_error_string();
int64_t generate_conn_code();

using er_e = error_code_e;
using Ev = std::expected<void, Error>;
}  // namespace bsm

#endif
