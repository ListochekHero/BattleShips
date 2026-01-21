#include "utility.h"

namespace bsm {

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
bool is_file_exist(const std::string& filename) {
  return std::filesystem::exists(filename);
}

std::string c_error_string() { return std::system_category().message(errno); }

int64_t generate_conn_code() {
  std::time_t now = std::time(nullptr);
  std::string conn_code_s{std::to_string(now)};
  std::string short_code_s{conn_code_s.substr(conn_code_s.length() - 5, 5)};
  int64_t conn_code_i{std::strtol(short_code_s.c_str(), nullptr, 10)};
  return conn_code_i;
}
}  // namespace bsm
