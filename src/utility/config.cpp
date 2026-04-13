#include "config.h"

#include "logger.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace bsm {

Config::Config() { init(); }

Config& Config::instance() {
  static Config instance;
  return instance;
}

void Config::init() {
  std::ifstream config_file("bs.conf", std::ios::in);
  if (config_file.is_open()) {
    // std::cout << "no-error" << std::endl;

    std::string option_line;
    while (std::getline(config_file, option_line)) {
      std::string key;
      std::string value;
      std::istringstream stream_option_line(option_line);
      if (!(stream_option_line >> key >> value)) {
        break;
      }
      for (size_t opt{0}; opt < MAX_OPTIONS; ++opt) {
        if ((this->configopt_sa[static_cast<int>(opt)].optname == key)) {
          this->configopt_sa[static_cast<int>(opt)].val =
              strtol(value.c_str(), nullptr, 10);
        }
      }
    }
  } else {
    LOG("error");
    std::cout << "error" << std::endl;
  }
}

int Config::get_line(std::string line) { return 8000; }

} // namespace bsm
