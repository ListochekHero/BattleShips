#ifndef LOGGER_H
#define LOGGER_H

#include "error.h"
#include <unistd.h>

#include <expected>
#include <format>
#include <fstream>
#include <mutex>
#include <string>

namespace bsm {

void LOG(const std::string_view message_to_log);

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
  static Logger& instance();
  std::expected<void, std::string> log(const std::string_view message);
  std::expected<void, std::string> init(const std::string& program_name);

private:
  std::ofstream log_file;
  std::mutex log_mutex;
  std::string program_name;

  Logger() = default;
  ~Logger();
  bool is_logfile_valid();
  std::string get_current_time();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
};
} // namespace bsm

#endif
