#ifndef CONFIG_H
#define CONFIG_H

#include <array>
#include <string>
#include <variant>

#define MAX_OPTIONS 1

namespace bsm {
class Config {
public:
  static Config& instance();
  Config();
  ~Config() = default;
  void init();
  int get_line(std::string);
  struct configopt_s {
    const std::string optname;
    std::variant<long, std::string> val;
  };
  std::array<configopt_s, 1> configopt_sa{{"port", 0}};

private:
};

} // namespace bsm

#endif
