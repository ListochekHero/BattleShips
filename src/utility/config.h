#ifndef CONFIG_H
#define CONFIG_H

#include <array>
#include <string>
#include <variant>

#define MAX_OPTIONS 1

namespace bsm {
class Config {
public:
  static auto instance() -> Config&;
  Config();
  ~Config() = default;
  void init();
  auto get_line(std::string) -> int;
  struct configopt_s {
    const std::string optname;
    std::variant<long, std::string> val;
  };
  std::array<configopt_s, 1> configopt_sa{{{.optname = "port", .val = 0}}};

private:
};

} // namespace bsm

#endif
