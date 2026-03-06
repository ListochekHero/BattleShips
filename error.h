#ifndef ERROR_H
#define ERROR_H

#include <expected>
#include <format>
#include <source_location>
#include <string>
#include <vector>

namespace bsm {

enum class user_error_e {
  GENERIC,
  CANT_CREATE_LOBBY,
  CANT_JOIN_LOBBY,
  UNKNOWN_COMMAND
};
using us_e = user_error_e;

struct Error {
  std::vector<std::string> backtrace;
  template <typename Self>
  auto&&
  add_context(this Self&& self, std::string msg,
              std::source_location loc = std::source_location::current()) {
    self.backtrace.push_back(std::format("{}:{}\n\t\tin {}:\n\t\t{}",
                                         loc.file_name(), loc.line(),
                                         loc.function_name(), msg));
    return std::forward<Self>(self);
  }
  std::string full_report() const;
};
using Ev = std::expected<void, Error>;

std::string c_error_string();
Error make_error_c(std::string message);
std::string_view user_message(user_error_e user);

} // namespace bsm
#endif
