#ifndef SOCKET_HANDLING_H
#define SOCKET_HANDLING_H

#include <arpa/inet.h>
#include <cstddef>
#include <fcntl.h>
#include <limits>
#include <optional>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <expected>
#include <vector>

#include "utility.h"

#define BACKLOG 10
#define BUFF_SIZE 1024 //  add this magic number to config
namespace bsm {

enum class socket_status_e { EMPTY, ALIVE, CLOSED, TRANSFERED };
enum class message_type_e : uint8_t { DEFAULT, SOCKET, CONN_CODE };
enum class message_status_e { EMPTY, WOULDBLOCK, DISCONNECTED, DATA };

struct MsgHeader {
  message_type_e msg_type{message_type_e::DEFAULT};
  uint64_t payload_count{0};
};

struct OutgoingMessage {
  std::vector<std::string_view> payloads;
  message_type_e msg_type{message_type_e::DEFAULT};
  std::optional<int> socket{std::nullopt};
};

struct ReadResult {
  message_status_e status{message_status_e::EMPTY};
  message_type_e msg_type{message_type_e::DEFAULT};
  std::string payload{std::string(1024, '\0')};
  std::optional<int> socket{std::nullopt};
};

class SocketHandler {
public:
  SocketHandler() = default;
  SocketHandler(int socket_fd);
  ~SocketHandler();
  SocketHandler(SocketHandler&&);
  SocketHandler& operator=(SocketHandler&&);
  Ev setup_listener(int port);
  int get_socket() const;
  socket_status_e get_socket_status() const;
  void set_socket_status(socket_status_e socket_status);
  std::expected<std::vector<int>, Error> accept_connections() const;
  std::expected<ReadResult, Error> read_user_input();
  Ev write_to_user(const OutgoingMessage& msg) const;
  Ev remove_cloexec();
  void reset_with_new(int new_socket);
  void reset_to_empty();

  std::string nick_name{*generate_name()};

  SocketHandler(const SocketHandler&) = delete;
  SocketHandler& operator=(const SocketHandler&) = delete;

private:
  int socket_{-1};
  socket_status_e socket_status_{socket_status_e::EMPTY};

  void swap(SocketHandler& left_sh, SocketHandler& r_sh);
  void close_socket();
};

class EpollHandler {
public:
  EpollHandler() = default;
  ~EpollHandler();
  Ev init();
  Ev add_socket(SocketHandler& socket_handler);
  Ev remove_socket(SocketHandler& socket_handler);
  std::expected<std::vector<size_t>, Error>
  wait_for_events(size_t max_events) const;

private:
  int epollfd_ = 0;
};

} // namespace bsm

#endif
