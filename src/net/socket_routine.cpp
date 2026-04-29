#include "socket_routine.h"

#include "utility/logger.h"

#include <cstddef>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace bsm {

SocketHandler::SocketHandler(int socket_fd)
    : socket_{socket_fd}, socket_status_{socket_status_e::ALIVE} {}

SocketHandler::~SocketHandler() { close_socket(); }

SocketHandler::SocketHandler(SocketHandler&& sock_hndl) noexcept
    : socket_{std::exchange(sock_hndl.socket_, -1)},
      socket_status_{sock_hndl.socket_status_.load()} {
  sock_hndl.socket_status_.store(socket_status_e::EMPTY);
}

auto SocketHandler::operator=(SocketHandler&& sock_hndl) noexcept
    -> SocketHandler& {
  if (this != &sock_hndl) {
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

auto SocketHandler::receive_message() -> std::expected<ReadResult, Error> {
  MsgHeader hdr;
  ReadResult result;
  struct iovec iov[2]{
      {.iov_base = &hdr, .iov_len = sizeof(hdr)},
      {.iov_base = result.payload.data(), .iov_len = result.payload.size()},
  };
  struct msghdr msg{};
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  char buf[CMSG_SPACE(sizeof(int))];
  msg.msg_control = buf;
  msg.msg_controllen = sizeof(buf);

  ssize_t n = recvmsg(socket_, &msg, 0);
  if (n > 0) {
    result.payload.resize(n - sizeof(hdr));
    result.msg_type = hdr.msg_type;
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg != nullptr && cmsg->cmsg_level == SOL_SOCKET &&
        cmsg->cmsg_type == SCM_RIGHTS) {
      int socket;
      std::memcpy(&socket, CMSG_DATA(cmsg), sizeof(int));
      result.socket = socket;
    }
    LOG(std::format("Message received: {}", result.payload));
    result.status = message_status_e::DATA;
  } else if (n == 0) {
    result.status = message_status_e::DISCONNECTED;
    socket_status_ = socket_status_e::CLOSED;
  } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
    result.status = message_status_e::WOULDBLOCK;
  } else {
    return std::unexpected(make_error_c("Read error"));
  }
  return result;
}

auto SocketHandler::send_message(const OutgoingMessage& msg) const
    -> std::optional<Error> {
  LOG(std::format("Message for user: {}", msg.payloads));
  MsgHeader hdr{.msg_type = msg.msg_type, .payload_count = msg.payloads.size()};
  std::vector<iovec> iov;
  iov.reserve(msg.payloads.size());
  iov.push_back({.iov_base = &hdr, .iov_len = sizeof(hdr)});
  for (auto message : msg.payloads) {
    iov.push_back({.iov_base = const_cast<char*>(message.data()),
                   .iov_len = message.size()});
  }
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
  close_socket();
  socket_ = new_socket;
  socket_status_ = socket_status_e::ALIVE;
}

void SocketHandler::reset_to_empty() {
  close_socket();
  socket_status_ = socket_status_e::EMPTY;
}

void SocketHandler::swap(SocketHandler& left_sh, SocketHandler& r_sh) {
  // зробити свап (?)
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
