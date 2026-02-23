#ifndef UTILITY_H
#define UTILITY_H

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include "error.h"

namespace bsm {

class ConnectionView;
struct ReadResult;
enum class end_point_e : uint8_t;

enum class command_status_e { CONTINUE, TERMINATE };
using cmd_se = command_status_e;

struct CommandStatus {
  command_status_e command_status_v;
  std::optional<Error> error{std::nullopt};
  std::optional<us_e> user_code{us_e::GENERIC};
};

struct CommandContext {
  ConnectionView& client;
  ReadResult& message;
  end_point_e peer;
};

bool is_file_exist(const std::string& filename);
int64_t generate_conn_code();
std::expected<std::string, Error> parse(const std::string& message);
std::expected<std::string, Error> generate_name();
void success_or_terminate(Ev&& r);

template <typename T, typename E> void drop_result(std::expected<T, E>&&) {}
} // namespace bsm

#endif
