#include "utility.h"

namespace bsm {

bool is_file_exist(const std::string& filename) {
  return std::filesystem::exists(filename);
}

std::string c_error_string() { return std::system_category().message(errno); }

int64_t generate_conn_code() {
  std::time_t now = std::time(nullptr);
  std::string conn_code_s{std::to_string(now)};
  std::string short_code_s{conn_code_s.substr(conn_code_s.length() - 5, 5)};
  int64_t conn_code_i{std::stol(short_code_s)};
  return conn_code_i;
}

}  // namespace bsm
