#include "socket_routine.h"
#include "logger.h"
#include <cstddef>
#include <unistd.h>
#include <utility>

namespace bsm {

SocketHandler::SocketHandler(int socket_fd)
    : socket_{socket_fd}, socket_status_{socket_status_e::ALIVE} {}

SocketHandler::~SocketHandler() { close_socket(); }

SocketHandler::SocketHandler(SocketHandler&& sock_hndl)
    : socket_{std::exchange(sock_hndl.socket_, -1)},
      socket_status_{sock_hndl.socket_status_.load()} {
  sock_hndl.socket_status_.store(socket_status_e::EMPTY);
}

SocketHandler& SocketHandler::operator=(SocketHandler&& sock_hndl) {
  if (this != &sock_hndl) {
    close_socket();
    socket_ = std::exchange(sock_hndl.socket_, -1);
    socket_status_.store(sock_hndl.socket_status_.load());
    sock_hndl.socket_status_.store(socket_status_e::EMPTY);
  }
  return *this;
}

Ev SocketHandler::setup_listener(int port) {
  if (!socket_)
    return std::unexpected(make_error_c("Socket is already exist"));

  socket_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (socket_ == -1)
    return std::unexpected(make_error_c("Error creating socket"));

  int opt = 1;
  if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    return std::unexpected(make_error_c("Error setting socket options"));

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) ==
      -1) {
    close(socket_);
    return std::unexpected(make_error_c("Error binding socket"));
  }

  if (listen(socket_, BACKLOG) == -1) {
    close(socket_);
    return std::unexpected(make_error_c("Error listening on socket"));
  }
  socket_status_ = socket_status_e::ALIVE;
  return {};
}

Ev SocketHandler::setup_client() {
  struct sockaddr_in server_addr;
  if ((socket_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return std::unexpected(Error{{"Socket creation error"}});
  }
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8000);
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  if (connect(socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) <
      0) {
    return std::unexpected(Error{{"Connection to the server failed"}});
  }
  socket_status_ = socket_status_e::ALIVE;
  return {};
}

int SocketHandler::get_socket() const { return socket_; }

socket_status_e SocketHandler::get_socket_status() const {
  return socket_status_;
}

void SocketHandler::set_socket_status(socket_status_e socket_status) {
  socket_status_ = socket_status;
}

std::expected<std::vector<int>, Error>
SocketHandler::accept_connections() const {
  std::vector<int> new_clients;
  while (true) {
    int client_fd =
        accept4(socket_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else {
        return std::unexpected(
            make_error_c("Error while accepting new connection"));
      }
    }
    new_clients.push_back(client_fd);
  }
  return new_clients;
}

std::expected<ReadResult, Error> SocketHandler::read_user_input() {
  MsgHeader hdr;
  ReadResult result;
  struct iovec iov[2]{{&hdr, sizeof(hdr)},
                      {result.payload.data(), result.payload.size()}};
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
    if (cmsg != NULL && cmsg->cmsg_level == SOL_SOCKET &&
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

Ev SocketHandler::write_to_user(const OutgoingMessage& msg) const {
  LOG(std::format("Message for user: {}", msg.payloads));
  MsgHeader hdr{.msg_type = msg.msg_type, .payload_count = msg.payloads.size()};
  std::vector<iovec> iov;
  iov.reserve(msg.payloads.size());
  iov.push_back({&hdr, sizeof(hdr)});
  for (auto message : msg.payloads) {
    iov.push_back({const_cast<char*>(message.data()), message.size()});
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
  if (sendmsg(socket_, &m, MSG_NOSIGNAL) == -1)
    return std::unexpected(make_error_c("Failed to send a message, error"));
  return {};
}

Ev SocketHandler::remove_cloexec() {
  int flags = fcntl(socket_, F_GETFD);
  if (flags == -1) {
    return std::unexpected(make_error_c("Cant get socket flags"));
  }
  flags &= ~FD_CLOEXEC;
  if (fcntl(socket_, F_SETFD, flags) == -1) {
    return std::unexpected(make_error_c("Cant set socket flags"));
  }
  return {};
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

Ev EpollHandler::init() {
  epollfd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epollfd_ == -1)
    return std::unexpected(make_error_c("Error creating epoll"));
  return {};
}

Ev EpollHandler::add_socket(SocketHandler& socket_handler, size_t slot) {
  struct epoll_event event;
  event.data.u64 = slot;
  // event.data.ptr = static_cast<void*>(socket_handler);
  event.events = EPOLLIN | EPOLLET;
  if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, socket_handler.get_socket(), &event) ==
      -1)
    return std::unexpected(make_error_c("Error adding socket to epoll"));
  return {};
}

Ev EpollHandler::remove_socket(SocketHandler& socket_handler) {
  if (epoll_ctl(epollfd_, EPOLL_CTL_DEL, socket_handler.get_socket(), NULL) ==
      -1)
    return std::unexpected(make_error_c("Error removing socket from epoll"));
  return {};
}

std::expected<std::vector<size_t>, Error>
EpollHandler::wait_for_events(size_t max_events) const {
  std::vector<struct epoll_event> events(max_events);
  ssize_t n = epoll_wait(epollfd_, events.data(), 10, -1);
  if (n == -1)
    return std::unexpected(make_error_c("Error in epoll_wait()"));
  std::vector<size_t> ready_slots;
  for (ssize_t i = 0; i < n; ++i) {
    ready_slots.push_back((events[i].data.u64));
  }
  return ready_slots;
}

} // namespace bsm
