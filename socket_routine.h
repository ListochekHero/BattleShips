#ifndef SOCKET_HANDLING_H
#define SOCKET_HANDLING_H

#include <arpa/inet.h>
#include <fcntl.h>
#include <optional>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <expected>
#include <memory>
#include <utility>
#include <vector>

#include "logger.h"
#include "utility.h"

#define BACKLOG 10
#define BUFF_SIZE 1024 //  add this magic number to config
namespace bsm {

enum class socket_type_e { UNKNOWN = -1, SERVER, IPC, CLIENT, SPECTATOR };
enum class socket_status_e { EMPTY, ALIVE, CLOSED, TRANSFERED };
enum class message_type_e : uint8_t { DEFAULT, SOCKET, CONN_CODE };
enum class message_status_e { DATA, WOULDBLOCK, NONVALID};

struct MsgHeader {
  message_type_e msg_type;
  uint64_t payload_count;
};

struct OutgoingMessage {
  message_type_e msg_type;
  std::vector<std::string_view> payloads;
  std::optional<int> socket{std::nullopt};
};

struct ReadResult {
  message_status_e status;
  message_type_e msg_type;
  std::string payload{std::string(1024, '\0')};
  std::optional<int> socket;
};

class SocketHandler {
public:
  SocketHandler() = default;
  SocketHandler(int socket_fd);
  ~SocketHandler();
  SocketHandler(const SocketHandler&) = delete;
  SocketHandler(SocketHandler&&);
  SocketHandler& operator=(const SocketHandler&) = delete;
  SocketHandler& operator=(SocketHandler&&);
  Ev setup_listener(int port);
  int get_socket() const;
  socket_type_e get_socket_type() const;
  void set_socket_type(socket_type_e socket_type);
  std::expected<std::vector<std::unique_ptr<SocketHandler>>, Error>
  accept_connections() const;
  std::expected<ReadResult, Error> read_user_input();
  Ev write_to_user(const OutgoingMessage& msg) const;
  Ev remove_cloexec();
  std::string nick_name{*generate_name()};

  socket_status_e socket_status_v{socket_status_e::EMPTY};

private:
  int socket_fd{-1};
  socket_type_e socket_type_v{-1};

  void swap(SocketHandler& left_sh, SocketHandler& r_sh);
  void close_socket();
};

class EpollHandler {
public:
  EpollHandler() = default;
  ~EpollHandler();
  Ev init();
  Ev add_socket(SocketHandler* const socket_handler);
  Ev remove_socket(const SocketHandler* const socket_handler);
  std::expected<std::vector<SocketHandler*>, Error>
  wait_for_events(size_t max_events) const;

private:
  int epollfd = 0;
};

} // namespace bsm

#endif
