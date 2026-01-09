#include "utility.h"
namespace bsm {

bool is_file_exist(const std::string& filename) {
  return std::filesystem::exists(filename);
}
std::string c_error_string() {
  return std::system_category().message(errno);
}
}  // namespace bsm
