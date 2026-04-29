#ifndef SOCKET_HANDLING_H
#define SOCKET_HANDLING_H

#include <arpa/inet.h>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <expected>
#include <vector>

#include "protocol/message_types.h"
#include "utility/utility.h"

#define BACKLOG 10
#define BUFF_SIZE 1024 //  add this magic number to config

namespace bsm {

enum class socket_status_e : uint8_t { EMPTY, ALIVE, CLOSED, TRANSFERED };

class SocketHandler {
public:
  SocketHandler() = default;
  SocketHandler(int socket_fd);
  ~SocketHandler();
  SocketHandler(SocketHandler&&) noexcept;
  auto operator=(SocketHandler&&) noexcept -> SocketHandler&;
  auto setup_listener(int port) -> std::optional<Error>;
  auto setup_client() -> std::optional<Error>;
  [[nodiscard]] auto get_socket() const -> int;
  [[nodiscard]] auto get_socket_status() const -> socket_status_e;
  void set_socket_status(socket_status_e socket_status);
  [[nodiscard]] auto accept_connections() const
      -> std::expected<std::vector<int>, Error>;
  auto receive_message() -> std::expected<ReadResult, Error>;
  [[nodiscard]] auto send_message(const OutgoingMessage& msg) const
      -> std::optional<Error>;
  auto remove_cloexec() const -> std::optional<Error>;
  void reset_with_new(int new_socket);
  void reset_to_empty();

  std::string nick_name{*generate_name()};

  SocketHandler(const SocketHandler&) = delete;
  auto operator=(const SocketHandler&) -> SocketHandler& = delete;

private:
  int socket_{-1};
  std::atomic<socket_status_e> socket_status_{socket_status_e::EMPTY};

  void swap(SocketHandler& left_sh, SocketHandler& r_sh);
  void close_socket();
};

class EpollHandler {
public:
  EpollHandler() = default;
  ~EpollHandler();
  auto init() -> std::optional<Error>;
  auto add_socket(SocketHandler& socket_handler, size_t slot) const
      -> std::optional<Error>;
  auto rearm_socket(const SocketHandler& socket_handler, size_t slot) const
      -> std::optional<Error>;
  auto remove_socket(SocketHandler& socket_handler) const
      -> std::optional<Error>;
  [[nodiscard]] auto wait_for_events(size_t max_events) const
      -> std::expected<std::vector<size_t>, Error>;

private:
  int epollfd_ = 0;
};

} // namespace bsm

#endif
