#include "logger.h"
namespace bsm {

void LOG(const std::string_view message_to_log) {
  Logger::instance().log(message_to_log).or_else([](const auto& error) -> Ev {
    std::cerr << error;
    return {};
  });
}

Logger& Logger::instance() {
  static Logger instance;
  return instance;
}

std::expected<void, std::string> Logger::init(const std::string& program_name) {
  std::lock_guard<std::mutex> guard(log_mutex);
  this->program_name = program_name;
  log_file.open(std::format("{}.log", program_name), std::ios::app);
  if (!is_logfile_valid()) return std::unexpected("Log file failed to open");

  return {};
}
std::expected<void, std::string> Logger::log(const std::string_view message) {
  std::lock_guard<std::mutex> guard(log_mutex);
  if (!is_logfile_valid()) return std::unexpected("Log file failed to open");
  log_file << "[" << get_current_time() << "] " << "[" << program_name << "] "
           << "[PID: " << getpid() << "] " << message << std::endl;
  return {};
}

Logger::~Logger() {
  if (log_file.is_open()) {
    log_file.close();
  }
}

bool Logger::is_logfile_valid() { return this->log_file.is_open(); }

std::string Logger::get_current_time() {
  std::time_t now = std::time(nullptr);
  std::tm* local_time = std::localtime(&now);
  std::stringstream time_stream;
  time_stream << (local_time->tm_year + 1900) << "-" << (local_time->tm_mon + 1)
              << "-" << local_time->tm_mday << " " << local_time->tm_hour << ":"
              << local_time->tm_min << ":" << local_time->tm_sec;
  return time_stream.str();
}

}  // namespace bsm
