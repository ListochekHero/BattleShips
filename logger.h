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
  void log(const std::string& message);

 private:
  std::ofstream log_file;
  std::mutex log_mutex;
  std::string program_name;

  Logger(const std::string& program_name);
  ~Logger();
  std::string get_current_time();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
};
}  // namespace BattleShipsMain

#endif