#ifndef ERROR_H
#define ERROR_H

#include <expected>
#include <format>
#include <optional>
#include <source_location>
#include <string>
#include <vector>

namespace bsm {

enum class user_error_e : uint8_t {
  GENERIC,
  CANT_CREATE_LOBBY,
  CANT_JOIN_LOBBY,
  UNKNOWN_COMMAND,
};
using us_e = user_error_e;

struct Error {
  std::vector<std::string> backtrace;
  std::optional<int> loc_errno{std::nullopt};
  template <typename Self>
  auto add_context(this Self&& self, std::string msg,
                   std::source_location loc = std::source_location::current())
      -> auto&& {
    self.backtrace.push_back(std::format("{}:{}\n\t\tin {}:\n\t\t{}",
                                         loc.file_name(), loc.line(),
                                         loc.function_name(), msg));
    return std::forward<Self>(self);
  }
  [[nodiscard]] auto full_report() const -> std::string;
};
using Ev = std::expected<void, Error>;

auto c_error_string() -> std::string;
auto make_error_c(std::string message) -> Error;
auto user_message(user_error_e user) -> std::string_view;

} // namespace bsm
#endif
