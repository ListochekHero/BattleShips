#ifndef LOGGER_H
#define LOGGER_H

#include <unistd.h>

#include <ctime>
#include <expected>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include "utility.h"

namespace bsm {

void LOG(const std::string_view message_to_log);

inline auto log_and_forward = [](const auto& error) -> std::string {
  LOG(error);
  return error;
};

inline auto with_log(std::string_view context) {
  return [context](const std::string& error) -> std::string {
    std::string detailed_error{std::format("[{}]: [{}]", context, error)};
    log_and_forward(detailed_error);
    return detailed_error;
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
}  // namespace bsm

#endif
