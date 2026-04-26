#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include "utility/error.h"

#include <cstdint>
#include <expected>
#include <string>

namespace bsm {

struct JoinLobbyCode {
  std::string string_code;
  int64_t int_code;

  static auto parse(const std::string& message)
      -> std::expected<JoinLobbyCode, Error>;
};

} // namespace bsm
#endif
