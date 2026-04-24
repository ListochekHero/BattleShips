#include "utility.h"

#include "error.h"

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>

namespace bsm {

auto is_file_exist(const std::string& filename) -> bool {
  return std::filesystem::exists(filename);
}

auto generate_conn_code() -> int64_t {
  std::time_t now = std::time(nullptr);
  std::string conn_code_s{std::to_string(now)};
  std::string short_code_s{conn_code_s.substr(conn_code_s.length() - 5, 5)};
  int64_t conn_code_i{std::strtol(short_code_s.c_str(), nullptr, 10)};
  return conn_code_i;
}

auto parse(const std::string& message) -> std::expected<std::string, Error> {
  std::string string_code;
  size_t pos{message.find_first_of(' ')};
  if (pos != std::string::npos) {
    string_code = {message, ++pos};
  } else {
    return std::unexpected(Error{.backtrace = {"Cant parse chat message"}});
  };
  return string_code;
}

auto generate_name() -> std::expected<std::string, Error> {
  std::string new_name = "user" + std::to_string(generate_conn_code());
  return new_name;
}

} // namespace bsm
