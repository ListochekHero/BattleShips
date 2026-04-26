// IWYU pragma: no_include <vector>
#include "data_storage.h"

#include <cstdlib>

namespace bsm {

auto JoinLobbyCode::parse(const std::string& message)
    -> std::expected<JoinLobbyCode, Error> {
  int64_t int_code{std::strtol(message.c_str(), nullptr, 10)};
  std::string string_code;
  if (!int_code) {
    size_t pos{message.find_last_of(' ')};
    if (pos != std::string::npos) {
      string_code = {message, ++pos};
      int_code = std::strtol(string_code.c_str(), nullptr, 10);
    } else {
      return std::unexpected(Error{.backtrace = {"Cant parse connetion code"}});
    };
  } else {
    string_code = std::to_string(int_code);
  }
  return JoinLobbyCode{.string_code = string_code, .int_code = int_code};
}
} // namespace bsm
