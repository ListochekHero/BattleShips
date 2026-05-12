#include "socket_routine.h"

#include "protocol/message_defs.h"
#include "protocol/message_types.h"
#include "utility/logger.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <optional>
#include <ranges>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace bsm {

SocketHandler::SocketHandler(int socket_fd)
    : socket_{socket_fd}, socket_status_{socket_status_e::ALIVE} {}

SocketHandler::~SocketHandler() {
  destroy_co_handle();
  close_socket();
}

SocketHandler::SocketHandler(SocketHandler&& sock_hndl) noexcept
    : socket_{std::exchange(sock_hndl.socket_, -1)},
      socket_status_{sock_hndl.socket_status_.load()} {
  sock_hndl.socket_status_.store(socket_status_e::EMPTY);
}

auto SocketHandler::operator=(SocketHandler&& sock_hndl) noexcept
    -> SocketHandler& {
  if (this != &sock_hndl) {
    destroy_co_handle();
    close_socket();
    socket_ = std::exchange(sock_hndl.socket_, -1);
    socket_status_.store(sock_hndl.socket_status_.load());
    sock_hndl.socket_status_.store(socket_status_e::EMPTY);
  }
  return *this;
}

auto SocketHandler::setup_listener(int port) -> std::optional<Error> {
  if (socket_ != -1) {
    return make_error_c("Socket is already exist");
  }
  socket_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (socket_ == -1) {
    return make_error_c("Error creating socket");
  }
  int opt = 1;
  if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    return make_error_c("Error setting socket options");
  }
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(socket_, reinterpret_cast<struct sockaddr*>(&server_addr),
           sizeof(server_addr)) == -1) {
    close(socket_);
    return make_error_c("Error binding socket");
  }

  if (listen(socket_, BACKLOG) == -1) {
    close(socket_);
    return make_error_c("Error listening on socket");
  }
  socket_status_ = socket_status_e::ALIVE;
  return std::nullopt;
}

auto SocketHandler::setup_client() -> std::optional<Error> {
  struct sockaddr_in server_addr;
  socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ < 0) {
    return Error{.backtrace = {"Socket creation error"}};
  }
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8000);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  if (connect(socket_, reinterpret_cast<struct sockaddr*>(&server_addr),
              sizeof(server_addr)) < 0) {
    return Error{.backtrace = {"Connection to the server failed"}};
  }
  int flags = fcntl(socket_, F_GETFL, 0);
  fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
  socket_status_ = socket_status_e::ALIVE;
  return std::nullopt;
}

auto SocketHandler::get_socket() const -> int { return socket_; }

auto SocketHandler::get_socket_status() const -> socket_status_e {
  return socket_status_;
}

void SocketHandler::set_socket_status(socket_status_e socket_status) {
  socket_status_ = socket_status;
}

auto SocketHandler::is_socket_alive() -> bool {
  return socket_status_ == socket_status_e::ALIVE;
}

auto SocketHandler::accept_connections() const
    -> std::expected<std::vector<int>, Error> {
  std::vector<int> new_clients;
  while (true) {
    int client_fd =
        accept4(socket_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      return std::unexpected(
          make_error_c("Unable to accept new connections, accept4 error:"));
    }
    new_clients.push_back(client_fd);
  }
  return new_clients;
}

auto SocketHandler::receive_message() -> std::expected<ReceivedMessage, Error> {
  MessageHeader hdr;
  ReceivedMessage received_messaage;
  received_messaage.payload.resize(1024);
  struct iovec iov[2]{
      {.iov_base = &hdr, .iov_len = sizeof(hdr)},
      {
          .iov_base = received_messaage.payload.data(),
          .iov_len = received_messaage.payload.size(),
      },
  };
  struct msghdr msg{};
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  char buf[CMSG_SPACE(sizeof(int))];
  msg.msg_control = buf;
  msg.msg_controllen = sizeof(buf);

  ssize_t n = recvmsg(socket_, &msg, 0);
  if (n > 0) {
    received_messaage.payload.resize(n - sizeof(hdr));
    received_messaage.type = hdr.type;
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg != nullptr && cmsg->cmsg_level == SOL_SOCKET &&
        cmsg->cmsg_type == SCM_RIGHTS) {
      int socket;
      std::memcpy(&socket, CMSG_DATA(cmsg), sizeof(int));
      received_messaage.socket = socket;
    }
    LOG(std::format("Message received: {}", received_messaage.payload));
    received_messaage.status = message_status_e::DATA;
  } else if (n == 0) {
    received_messaage.status = message_status_e::DISCONNECTED;
    socket_status_ = socket_status_e::CLOSED;
  } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
    received_messaage.status = message_status_e::WOULDBLOCK;
  } else {
    return std::unexpected(make_error_c("Read error"));
  }
  return received_messaage;
}

auto SocketHandler::get_message() -> std::optional<ReceivedMessage> {
  if (!receive_handle_) {
    set_receive_handle();
  }
  receive_handle_->resume();
  auto received_message{std::move(receive_handle_->promise().receive_result)};
  if (receive_handle_->done()) {
    destroy_co_handle();
  }
  if (!received_message) {
    LOG(received_message.error().full_report());
    return std::nullopt;
  }
  return std::move(*received_message);
}

void SocketHandler::set_receive_handle() {
  receive_handle_ = co_receive_from_socket();
}

auto SocketHandler::co_receive_from_socket() -> receive_co_handle {
  std::vector<char> receive_buffer;
  receive_buffer.resize(1024); // add this to Config
  uint16_t free_beggins{0};
  while (true) {
    struct iovec iodata_vector{
        .iov_base = receive_buffer.data() + free_beggins,
        .iov_len = receive_buffer.size() - free_beggins,

    };
    struct msghdr raw_message{};
    raw_message.msg_iov = &iodata_vector;
    raw_message.msg_iovlen = 1;
    char control_buf[CMSG_SPACE(sizeof(int))]; // NOLINT
    raw_message.msg_control = control_buf;
    raw_message.msg_controllen = sizeof(control_buf);

    ssize_t bytes_received = recvmsg(socket_, &raw_message, 0);
    ReceivedMessage received_message;
    if (bytes_received > 0) {
      bytes_received += free_beggins;
      free_beggins = bytes_received;
      if (auto socket{process_control_message(raw_message)}) {
        received_message.socket = socket;
      }
      const char* data_beggins{
          static_cast<char*>(receive_buffer.data()),
      };
      MessageHeader header;
      while (bytes_received > 0) {
        if (header.type == message_type_e::NONE) {
          if (!process_message_header(data_beggins, bytes_received, header)) {
            break;
          }
        }
        if (process_message_payload(received_message, data_beggins,
                                    bytes_received, header.payload_size)) {
          received_message.type =
              std::exchange(header.type, message_type_e::NONE);
          LOG(std::format("Message received: {}", received_message.payload));
          co_yield std::move(received_message);
        } else {
          break;
        }
      }
      move_leftover_to_beginning(receive_buffer, data_beggins, bytes_received,
                                 free_beggins);
    } else if (bytes_received == 0) {
      received_message.status = message_status_e::DISCONNECTED;
      socket_status_ = socket_status_e::CLOSED;
      co_return received_message;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      received_message.status = message_status_e::WOULDBLOCK;
      co_yield std::move(received_message);
    } else {
      co_return std::unexpected(make_error_c("Read error"));
    }
  }
}

auto SocketHandler::process_control_message(const struct msghdr& raw_message)
    -> std::optional<int> {
  struct cmsghdr* control_message = CMSG_FIRSTHDR(&raw_message);
  if (control_message != nullptr && control_message->cmsg_level == SOL_SOCKET &&
      control_message->cmsg_type == SCM_RIGHTS) {
    int socket;
    std::memcpy(&socket, CMSG_DATA(control_message), sizeof(int));
    return socket;
  }
  return std::nullopt;
}

auto SocketHandler::process_message_header(const char*& data_beggins,
                                           ssize_t& bytes_received,
                                           MessageHeader& header) -> bool {
  auto header_size{sizeof(header)};
  if (std::cmp_greater_equal(bytes_received, header_size)) {
    memcpy(&header, data_beggins, header_size);
    data_beggins += header_size;
    bytes_received -= header_size;
    return true;
  }
  return false;
}

auto SocketHandler::process_message_payload(ReceivedMessage& received_message,
                                            const char*& data_beggins,
                                            ssize_t& bytes_received,
                                            uint64_t payload_size) -> bool {
  if (std::cmp_greater_equal(bytes_received, payload_size)) {
    received_message.payload = std::string{data_beggins, payload_size};
    data_beggins += payload_size;
    bytes_received -= payload_size;
    received_message.status = message_status_e::DATA;
    return true;
  }
  return false;
}

void SocketHandler::move_leftover_to_beginning(
    std::vector<char>& receive_buffer, const char* data_beggins,
    ssize_t bytes_received, uint16_t& free_beggins) {
  memmove(receive_buffer.data(), data_beggins, bytes_received);
  free_beggins = bytes_received;
}

auto SocketHandler::send_message(const OutgoingMessage& msg) const
    -> std::optional<Error> {
  // LOG(std::format("Message for user: {}", msg.payload));
  MessageHeader hdr{.type = msg.type, .payload_size = msg.payload.size()};
  std::vector<iovec> iov;
  iov.reserve(2);
  iov.push_back({.iov_base = &hdr, .iov_len = sizeof(hdr)});
  iov.push_back({
      .iov_base = const_cast<char*>(msg.payload.data()),
      .iov_len = msg.payload.size(),
  });
  struct msghdr m{};
  m.msg_iov = iov.data();
  m.msg_iovlen = iov.size();
  char buf[CMSG_SPACE(sizeof(int))];
  struct cmsghdr* cmsg;
  if (msg.socket) {
    m.msg_control = buf;
    m.msg_controllen = sizeof(buf);
    cmsg = CMSG_FIRSTHDR(&m);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    int socket = msg.socket.value();
    std::memcpy(CMSG_DATA(cmsg), &socket, sizeof(int));
  }
  if (sendmsg(socket_, &m, MSG_NOSIGNAL) == -1) {
    return make_error_c("Unable to send message, sendmsg() error:");
  }
  return std::nullopt;
}

auto SocketHandler::remove_cloexec() const -> std::optional<Error> {
  int flags = fcntl(socket_, F_GETFD);
  if (flags == -1) {
    return make_error_c("Cant get socket flags");
  }
  flags &= ~FD_CLOEXEC;
  if (fcntl(socket_, F_SETFD, flags) == -1) {
    return make_error_c("Cant set socket flags");
  }
  return std::nullopt;
}

void SocketHandler::reset_with_new(int new_socket) {
  destroy_co_handle();
  close_socket();
  socket_ = new_socket;
  socket_status_ = socket_status_e::ALIVE;
}

void SocketHandler::reset_to_empty() {
  destroy_co_handle();
  close_socket();
  socket_status_ = socket_status_e::EMPTY;
}

void SocketHandler::swap(SocketHandler& left_sh, SocketHandler& r_sh) {
  // зробити свап (?)
}

void SocketHandler::destroy_co_handle() {
  if (receive_handle_) {
    receive_handle_->destroy();
    receive_handle_ = std::nullopt;
  }
}

void SocketHandler::close_socket() {
  if (socket_ != -1) {
    close(socket_);
    socket_ = -1;
  }
}

EpollHandler::~EpollHandler() { close(epollfd_); }

auto EpollHandler::init() -> std::optional<Error> {
  epollfd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epollfd_ == -1) {
    return make_error_c("Error creating epoll");
  }
  return std::nullopt;
}

auto EpollHandler::add_socket(SocketHandler& socket_handler, size_t slot) const
    -> std::optional<Error> {
  struct epoll_event event;
  event.data.u64 = slot;
  // event.data.ptr = static_cast<void*>(socket_handler);
  event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
  if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, socket_handler.get_socket(), &event) ==
      -1) {
    return make_error_c("Error adding socket to epoll");
  }
  return std::nullopt;
}

auto EpollHandler::rearm_socket(const SocketHandler& socket_handler,
                                size_t slot) const -> std::optional<Error> {
  struct epoll_event event;
  event.data.u64 = slot;
  event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
  if (epoll_ctl(epollfd_, EPOLL_CTL_MOD, socket_handler.get_socket(), &event) ==
      -1) {
    return make_error_c("Error rearming socket in epoll");
  }
  return std::nullopt;
}

auto EpollHandler::remove_socket(SocketHandler& socket_handler) const
    -> std::optional<Error> {
  if (epoll_ctl(epollfd_, EPOLL_CTL_DEL, socket_handler.get_socket(), NULL) ==
      -1) {
    return make_error_c("Error removing socket from epoll");
  }
  return std::nullopt;
}

auto EpollHandler::wait_for_events(size_t max_events) const
    -> std::expected<std::vector<size_t>, Error> {
  std::vector<struct epoll_event> events(max_events);
  ssize_t n = epoll_wait(epollfd_, events.data(), 10, -1);
  if (n == -1) {
    return std::unexpected(
        make_error_c("Unable to get events from epoll, epoll_wait error:"));
  }
  std::vector<size_t> ready_slots;
  ready_slots.reserve(n);
  for (ssize_t i = 0; i < n; ++i) {
    ready_slots.push_back((events[i].data.u64));
  }
  return ready_slots;
}

} // namespace bsm
