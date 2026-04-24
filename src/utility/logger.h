#ifndef LOGGER_H
#define LOGGER_H

#include "error.h"

#include <expected>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace bsm {

void LOG(std::string_view message_to_log);

// std::unexpected<Error> log_and_replace();
// void log_and_ignore(const Error& error, std::string_view context);

inline auto log_and_forward = [](const auto& error) -> Error {
  LOG(error.message);
  return error;
};

inline auto with_log(std::string_view context) {
  return [context](auto&& error) -> Error {
    error.message = std::format("[{}]: [{}]", context, error.message);
    log_and_forward(error);
    return error;
  };
}

class Logger {
public:
  static auto instance() -> Logger&;
  auto log(std::string_view message) -> std::expected<void, std::string>;
  auto init(const std::string& program_name)
      -> std::expected<void, std::string>;

  Logger(const Logger&) = delete;
  auto operator=(const Logger&) -> Logger& = delete;

private:
  std::ofstream log_file;
  std::mutex log_mutex;
  std::string program_name;

  Logger() = default;
  ~Logger();
  auto is_logfile_valid() -> bool;
  static auto get_current_time() -> std::string;
};
} // namespace bsm

#endif
