#ifndef LOGGER_H
#define LOGGER_H

#include <unistd.h>

#include <ctime>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

namespace BattleShipsMain {
class Logger {
 public:
  static Logger& getInstance(const std::string& program_name);
  static Logger& getInstance();
  void log(const std::string& message);

 private:
  std::ofstream log_file;
  std::mutex log_mutex;
  std::string program_name;

  inline static bool instance_created = false;

  Logger(const std::string& program_name);
  Logger() = default;
  ~Logger();
  std::string get_current_time();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
};

#define LOG(message_to_log) \
  BattleShipsMain::Logger::getInstance().log(message_to_log)
}  // namespace BattleShipsMain

#endif