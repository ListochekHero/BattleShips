#include "socket_routine.h"

namespace bsm {

SocketHandler::SocketHandler(int socket_fd)
    : socket_fd{socket_fd}, socket_type_v{socket_type_e::CLIENT} {}

SocketHandler::~SocketHandler() { close_socket(); }

SocketHandler::SocketHandler(SocketHandler&& sock_hndl)
    : socket_fd{std::exchange(sock_hndl.socket_fd, -1)},
      socket_type_v{
          std::exchange(sock_hndl.socket_type_v, socket_type_e::UNKNOWN)} {}

SocketHandler& SocketHandler::operator=(SocketHandler&& sock_hndl) {
  if (this != &sock_hndl) {
    this->close_socket();
    socket_fd = std::exchange(sock_hndl.socket_fd, -1);
    socket_type_v =
        std::exchange(sock_hndl.socket_type_v, socket_type_e::UNKNOWN);
  }
  return *this;
}
Ev SocketHandler::setup_listener(int port) {
  if (!socket_fd)
    return std::unexpected(
        make_error_c(er_e::ALREADY_EXIST, "Socket is already exist"));

  socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (socket_fd == -1)
    return std::unexpected(make_error_c(er_e::SYSTEM, "Error creating socket"));

  int opt = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    return std::unexpected(
        make_error_c(er_e::SYSTEM, "Error setting socket options"));

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) ==
      -1) {
    close(socket_fd);
    return std::unexpected(make_error_c(er_e::SYSTEM, "Error binding socket"));
  }

  if (listen(socket_fd, BACKLOG) == -1) {
    close(socket_fd);
    return std::unexpected(
        make_error_c(er_e::SYSTEM, "Error listening on socket"));
  }
  socket_type_v = socket_type_e::SERVER;
  return {};
}

const int SocketHandler::get_socket() const { return socket_fd; }

socket_type_e SocketHandler::get_socket_type() const {
  return this->socket_type_v;
}

void SocketHandler::set_socket_type(socket_type_e socket_type) {
  this->socket_type_v = socket_type;
  return;
}

std::expected<std::vector<std::unique_ptr<SocketHandler>>, Error>
SocketHandler::accept_connections() const {
  std::vector<std::unique_ptr<SocketHandler>> new_clients;
  while (true) {
    int client_fd =
        accept4(socket_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else {
        return std::unexpected(
            make_error_c(er_e::SYSTEM, "Error while accepting new connection"));
      }
    }
    new_clients.emplace_back(std::make_unique<SocketHandler>(client_fd));
  }
  return new_clients;
}

std::expected<ReadResult, Error> SocketHandler::read_user_input() const {
  MsgHeader hdr;
  ReadResult result;
  struct iovec iov[2]{{&hdr, sizeof(hdr)},
                      {result.payload.data(), result.payload.size()}};
  struct msghdr msg{0};
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  char buf[CMSG_SPACE(sizeof(int))];
  msg.msg_control = buf;
  msg.msg_controllen = sizeof(buf);

  ssize_t n = recvmsg(this->socket_fd, &msg, 0);
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
    result.status = status_code_e::DATA;
  } else if (n == 0) {
    result.payload = "\\close";
    result.status = status_code_e::CLOSED;
  } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
    result.status = status_code_e::WOULDBLOCK;
  } else {
    return std::unexpected(make_error_c(er_e::SYSTEM, "Read error"));
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
  struct msghdr m{0};
  m.msg_iov = iov.data();
  m.msg_iovlen = iov.size();
  if (msg.socket) {
    char buf[CMSG_SPACE(sizeof(int))];
    m.msg_control = buf;
    m.msg_controllen = sizeof(buf);
    struct cmsghdr* cmsg;
    cmsg = CMSG_FIRSTHDR(&m);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    int socket = msg.socket.value();
    std::memcpy(CMSG_DATA(cmsg), &socket, sizeof(int));
  }
  if (sendmsg(this->socket_fd, &m, MSG_NOSIGNAL) == -1)
    return std::unexpected(
        make_error_c(er_e::SYSTEM, "Failed to send a message, error"));
  return {};
}

Ev SocketHandler::remove_cloexec() {
  int flags = fcntl(this->socket_fd, F_GETFD);
  if (flags == -1) {
    return std::unexpected(make_error_c(er_e::SYSTEM, "Cant get socket flags"));
  }
  flags &= ~FD_CLOEXEC;
  if (fcntl(this->socket_fd, F_SETFD, flags) == -1) {
    return std::unexpected(make_error_c(er_e::SYSTEM, "Cant set socket flags"));
  }
  return {};
}

void SocketHandler::swap(SocketHandler& left_sh, SocketHandler& r_sh) {
  // зробити свап (?)
}

void SocketHandler::close_socket() {
  if (socket_fd != -1) {
    close(socket_fd);
  }
}

EpollHandler::~EpollHandler() { close(epollfd); }

Ev EpollHandler::init() {
  epollfd = epoll_create1(EPOLL_CLOEXEC);
  if (epollfd == -1)
    return std::unexpected(make_error_c(er_e::SYSTEM, "Error creating epoll"));
  return {};
}

Ev EpollHandler::add_socket(SocketHandler* const socket_handler) {
  struct epoll_event event;
  event.data.ptr = static_cast<void*>(socket_handler);
  event.events = EPOLLIN | EPOLLET;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_handler->get_socket(), &event) ==
      -1)
    return std::unexpected(
        make_error_c(er_e::SYSTEM, "Error adding socket to epoll"));
  return {};
}

Ev EpollHandler::remove_socket(const SocketHandler* const socket_handler) {
  if (epoll_ctl(epollfd, EPOLL_CTL_DEL, socket_handler->get_socket(), NULL) ==
      -1)
    return std::unexpected(
        make_error_c(er_e::SYSTEM, "Error removing socket from epoll"));
  return {};
}

std::expected<std::vector<SocketHandler*>, Error> EpollHandler::wait_for_events(
    size_t max_events) const {
  std::vector<struct epoll_event> events(max_events);
  int n = epoll_wait(this->epollfd, events.data(), 10, -1);
  if (n == -1)
    return std::unexpected(make_error_c(er_e::SYSTEM, "Error in epoll_wait()"));
  std::vector<SocketHandler*> ready_fds;
  for (int i = 0; i < n; ++i) {
    ready_fds.push_back(static_cast<SocketHandler*>((events[i].data.ptr)));
  }
  return ready_fds;
}

}  // namespace bsm
