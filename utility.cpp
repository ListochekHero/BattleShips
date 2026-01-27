#include "utility.h"

namespace bsm {

std::string_view user_message(user_error_e user) {
  switch (user) {
  case user_error_e::GENERIC:
    return "Error occurred, please try again latter.";
  case user_error_e::CANT_CREATE_LOBBY:
    return "Cant create lobby, please try again latter.";
  case user_error_e::CANT_JOIN_LOBBY:
    return "Cant join lobby, please check connection code or try again.";
  // case er_e::ALREADY_EXIST:
  //   return "ALREADY_EXIST";
  // case er_e::INTERNAL:
  //   return "INTERNAL";
  // case er_e::SYSTEM:
  //   return "SYSTEM";
  default:
    return "Something unexpeceted happened";
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

std::expected<std::string, Error> parse(const std::string& message) {
  std::string string_code;
  size_t pos{message.find_last_of(' ')};
  if (pos != std::string::npos) {
    string_code = {message, ++pos};
  } else {
    return std::unexpected(
        Error{er_e::INVALID_ARGS, "Cant parse chat message"});
  };
  return string_code;
}
} // namespace bsm
