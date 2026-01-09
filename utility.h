#ifndef UTILITY_H
#define UTILITY_H

#include <filesystem>
#include <expected>
namespace bsm {

using Ev = std::expected<void, std::string>;

bool is_file_exist(const std::string& filename);

}  // namespace bsm

#endif
