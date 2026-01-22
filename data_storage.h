#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include <expected>
#include <filesystem>
#include <system_error>

#include "utility.h"

namespace bsm {
struct ConnectionCode {
  std::string string_code;
  int64_t int_code;

  static std::expected<ConnectionCode, Error> parse(const std::string& message);
};

struct LobbyProcess{
  pid_t pid;
  SocketHandler ipc_socket;
};
}  // namespace bsm
#endif
