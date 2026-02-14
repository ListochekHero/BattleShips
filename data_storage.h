#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include <expected>

#include "utility.h"

namespace bsm {

struct JoinLobbyCode {
  std::string string_code;
  int64_t int_code;

  static std::expected<JoinLobbyCode, Error> parse(const std::string& message);
};

} // namespace bsm
#endif
