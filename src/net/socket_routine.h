#ifndef SOCKET_HANDLING_H
#define SOCKET_HANDLING_H

#include <arpa/inet.h>
#include <atomic>
#include <coroutine>
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

struct receive_message_promise;
using receive_co_handle = std::coroutine_handle<receive_message_promise>;

struct receive_message_promise {
  std::expected<ReceivedMessage, Error> receive_result;
  auto get_return_object() -> receive_co_handle {
    return receive_co_handle::from_promise(*this);
  }
  static auto initial_suspend() noexcept -> std::suspend_always { return {}; }
  static auto final_suspend() noexcept -> std::suspend_always { return {}; }
  void unhandled_exception() {}
  auto yield_value(ReceivedMessage&& received_message) -> std::suspend_always {
    receive_result = std::move(received_message);
    return {};
  }
  void return_value(std::expected<ReceivedMessage, Error>&& co_result) {
    receive_result = std::move(co_result);
  }
};

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
  auto is_socket_alive() -> bool;
  void set_socket_status(socket_status_e socket_status);
  [[nodiscard]] auto accept_connections() const
      -> std::expected<std::vector<int>, Error>;
  auto receive_message() -> std::expected<ReceivedMessage, Error>;
  auto get_message() -> std::optional<ReceivedMessage>;
  void set_receive_handle();
  auto co_receive_from_socket() -> receive_co_handle;
  [[nodiscard]] auto send_message(const OutgoingMessage& msg) const
      -> std::optional<Error>;
  auto remove_cloexec() const -> std::optional<Error>;
  void reset_with_new(int new_socket);
  void reset_to_empty();
  static auto process_control_message(const struct msghdr& raw_message)
      -> std::optional<int>;
  static auto process_message_header(const char*& data_beggins,
                                     ssize_t& bytes_received,
                                     MessageHeader& header) -> bool;
  static auto process_message_payload(ReceivedMessage& received_message,
                                      const char*& data_beggins,
                                      ssize_t& bytes_received,
                                      uint64_t payload_size) -> bool;
  void move_leftover_to_beginning(std::vector<char>& receive_buffer,
                                  const char* data_beggins,
                                  ssize_t bytes_received,
                                  uint16_t& free_beggins);
  std::string nick_name{*generate_name()};

  SocketHandler(const SocketHandler&) = delete;
  auto operator=(const SocketHandler&) -> SocketHandler& = delete;

private:
  int socket_{-1};
  std::optional<receive_co_handle> receive_handle_{std::nullopt};
  std::atomic<socket_status_e> socket_status_{socket_status_e::EMPTY};

  void swap(SocketHandler& left_sh, SocketHandler& r_sh);
  void destroy_co_handle();
  void reset_co_handle();
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

template <>
struct std::coroutine_traits<bsm::receive_co_handle, bsm::SocketHandler&> {
  using promise_type = bsm::receive_message_promise;
};

#endif
