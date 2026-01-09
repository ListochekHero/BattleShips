#ifndef UTILITY_H
#define UTILITY_H

#include <expected>
#include <filesystem>
#include <system_error>

namespace bsm {

using Ev = std::expected<void, std::string>;

bool is_file_exist(const std::string& filename);

std::string c_error_string();

}  // namespace bsm

#endif
