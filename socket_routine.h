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

enum class socket_type_e { UNKNOWN = -1, SERVER, IPC, CLIENT, SPECTATOR };
enum class socket_status_e { EMPTY, ALIVE, CLOSED, TRANSFERED };
enum class message_type_e : uint8_t { DEFAULT, SOCKET, CONN_CODE };
enum class message_status_e { DATA, WOULDBLOCK, NONVALID };

struct MsgHeader {
  message_type_e msg_type;
  uint64_t payload_count;
};

struct OutgoingMessage {
  std::vector<std::string_view> payloads;
  message_type_e msg_type{message_type_e::DEFAULT};
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
  size_t get_occupied_slot();
  void set_occupied_slot(size_t slot);
  std::expected<std::vector<int>, Error> accept_connections() const;
  std::expected<ReadResult, Error> read_user_input();
  Ev write_to_user(const OutgoingMessage& msg) const;
  Ev remove_cloexec();
  void reset_with_new(int new_socket, socket_type_e type = socket_type_e::CLIENT);
  void reset_to_empty();
  std::string nick_name{*generate_name()};
  socket_status_e socket_status_v{socket_status_e::EMPTY};

private:
  int socket_fd{-1};
  socket_type_e socket_type_v{-1};
  size_t occupied_slot{std::numeric_limits<std::size_t>::max()};

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
  int epollfd = 0;
};

} // namespace bsm

#endif
