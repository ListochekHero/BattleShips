#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <mutex>
#include <string>

namespace BattleShipsMain {
class Logger {
 public:
  static Logger& getInstance();
    void log(const std::string& message);
 private:
 std::ofstream log_file;
 std::mutex log_mutex;
};
}  // namespace BattleShipsMain

#endif