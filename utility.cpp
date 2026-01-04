#include "utility.h"

bool bsm::is_file_exist(const std::string& filename) {
  return std::filesystem::exists(filename);
}
