#include "logger.h"

namespace BattleShipsMain {
Logger& Logger::getInstance(const std::string& program_name) {
  static Logger instance(program_name);
  return instance;
}
Logger& Logger::getInstance() {
  if(!instance_created){
    throw std::runtime_error("Logger must be initialized with a program name first!");
  }
  return getInstance("unused");
}
void Logger::log(const std::string& message) {
  std::lock_guard<std::mutex> guard(log_mutex);
  log_file << "[" << get_current_time() << "] " << "[" << program_name << "] "
           << "[PID: " << getpid() << "] " << message << std::endl;
}

Logger::Logger(const std::string& program_name) : program_name(program_name) {
  log_file.open("log.txt", std::ios::app);
  if (!log_file.is_open()) {
    throw std::runtime_error("Could not open log file");
  }
  instance_created = true;
}

Logger::~Logger() {
  if (log_file.is_open()) {
    log_file.close();
  }
}

std::string Logger::get_current_time() {
  std::time_t now = std::time(nullptr);
  std::tm* local_time = std::localtime(&now);
  std::stringstream time_stream;
  time_stream << (local_time->tm_year + 1900) << "-" << (local_time->tm_mon + 1)
              << "-" << local_time->tm_mday << " " << local_time->tm_hour << ":"
              << local_time->tm_min << ":" << local_time->tm_sec;
  return time_stream.str();
}
}  // namespace BattleShipsMain