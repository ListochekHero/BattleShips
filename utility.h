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
using er_e = error_code_e;

inline Error make_error(error_code_e code, std::string message) {
  return {code, std::move(message)};
}

std::string_view user_message(error_code_e code) {
  switch (code) {
    case er_e::INVALID_ARGS:
      return "INVALID_ARGS";
    case er_e::NOT_FOUND:
      return "NOT_FOUND";
    case er_e::PERMISSION_DENIED:
      return "PERMISSION_DENIED";
    case er_e::ALREADY_EXIST:
      return "ALREADY_EXIST";
    case er_e::INTERNAL:
      return "INTERNAL";
    case er_e::SYSTEM:
      return "SYSTEM";
    default:
      return "-no such code-";
  }
}

using Ev = std::expected<void, Error>;

bool is_file_exist(const std::string& filename);

std::string c_error_string();

}  // namespace bsm

#endif
