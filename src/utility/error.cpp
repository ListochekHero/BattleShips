#include "error.h"

namespace bsm {

auto Error::full_report() const -> std::string {
  std::string report{"Error Trace:\n"};
  for (auto it = backtrace.rbegin(); it != backtrace.rend(); ++it) {
    report += "\t->" + *it + "\n";
  }
  return report;
}

auto c_error_string() -> std::string {
  return std::system_category().message(errno);
}

auto make_error_c(std::string message) -> Error {
  return {.backtrace = {message, c_error_string()}, .loc_errno = errno};
}

auto user_message(user_error_e user) -> std::string_view {
  switch (user) {
  case user_error_e::GENERIC:
    return "Error occurred, please try again latter.";
  case user_error_e::CANT_CREATE_LOBBY:
    return "Cant create lobby, please try again latter.";
  case user_error_e::CANT_JOIN_LOBBY:
    return "Cant join lobby, please check connection code or try again.";
  case user_error_e::UNKNOWN_COMMAND:
    return "Unknown command, please check your input.";
  default:
    return "Something unexpeceted happened";
  }
}

} // namespace bsm
