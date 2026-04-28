#include "logger.h"

#include <ctime>
#include <iostream>
#include <sstream>
#include <unistd.h>

struct tm;

namespace bsm {

void LOG(const std::string_view message_to_log) {
  if (auto result = Logger::instance().log(message_to_log); !result) {
    std::cerr << result.error();
  }
}

auto Logger::instance() -> Logger& {
  static Logger instance;
  return instance;
}

auto Logger::init(const std::string& program_name)
    -> std::expected<void, std::string> {
  std::scoped_lock guard(log_mutex);
  this->program_name = program_name;
  log_file.open(std::format("{}.log", program_name), std::ios::app);
  if (!is_logfile_valid()) {
    return std::unexpected("Log file failed to open");
  }

  return {};
}
auto Logger::log(const std::string_view message)
    -> std::expected<void, std::string> {
  std::scoped_lock guard(log_mutex);
  if (!is_logfile_valid()) {
    return std::unexpected("Log file failed to open");
  }
  log_file << "[" << get_current_time() << "] " << "[" << program_name << "] "
           << "[PID: " << getpid() << "] " << message << '\n';
  return {};
}

Logger::~Logger() {
  if (log_file.is_open()) {
    log_file.close();
  }
}

auto Logger::is_logfile_valid() -> bool { return this->log_file.is_open(); }

auto Logger::get_current_time() -> std::string {
  std::time_t now = std::time(nullptr);
  std::tm* local_time = std::localtime(&now);
  std::stringstream time_stream;
  time_stream << (local_time->tm_year + 1900) << "-" << (local_time->tm_mon + 1)
              << "-" << local_time->tm_mday << " " << local_time->tm_hour << ":"
              << local_time->tm_min << ":" << local_time->tm_sec;
  return time_stream.str();
}

} // namespace bsm
