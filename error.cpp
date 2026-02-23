#include "error.h"
#include <format>
#include <source_location>

namespace bsm {

void Error::add_context(std::string msg, std::source_location loc) {
  backtrace.push_back(std::format("{}:{} in {}:{}", loc.file_name(), loc.line(),
                                  loc.function_name(), msg));
}
std::string Error::full_report() const {
  std::string report{"Error Trace:\n"};
  for (auto it = backtrace.rbegin(); it != backtrace.rend(); ++it) {
    report += "  ->" + *it + "\n";
  }
  return report;
}

std::string c_error_string() { return std::system_category().message(errno); }

Error make_error_c(std::string message) {
  return {.backtrace = {message, c_error_string()}};
}

std::string_view user_message(user_error_e user) {
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
