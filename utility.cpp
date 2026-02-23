#include "utility.h"
#include "logger.h"
#include <filesystem>
#include <string>

namespace bsm {

bool is_file_exist(const std::string& filename) {
  return std::filesystem::exists(filename);
}

int64_t generate_conn_code() {
  std::time_t now = std::time(nullptr);
  std::string conn_code_s{std::to_string(now)};
  std::string short_code_s{conn_code_s.substr(conn_code_s.length() - 5, 5)};
  int64_t conn_code_i{std::strtol(short_code_s.c_str(), nullptr, 10)};
  return conn_code_i;
}

std::expected<std::string, Error> parse(const std::string& message) {
  std::string string_code;
  size_t pos{message.find_first_of(' ')};
  if (pos != std::string::npos) {
    string_code = {message, ++pos};
  } else {
    return std::unexpected(Error{{"Cant parse chat message"}});
  };
  return string_code;
}

std::expected<std::string, Error> generate_name() {
  std::string new_name = "user" + std::to_string(generate_conn_code());
  return new_name;
}

void success_or_terminate(Ev&& r) {
  if (!r) {
    LOG(r.error().full_report());
    std::terminate();
  }
}

} // namespace bsm
