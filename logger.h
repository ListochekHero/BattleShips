#ifndef LOGGER_H
#define LOGGER_H

#include <unistd.h>

#include <ctime>
#include <expected>
#include <format>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

namespace bsm {
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
void LOG(const std::string_view message_to_log);
}  // namespace bsm

#endif
