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
std::expected<void, std::string> SocketHandler::setup_listenter(int port) {
  if (!socket_fd) return std::unexpected("Socket is already exist");

  socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (socket_fd == -1) return std::unexpected("Error creating socket");

  int opt = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    return std::unexpected("Error setting socket options");

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) ==
      -1) {
    close(socket_fd);
    return std::unexpected("Error binding socket");
  }

  if (listen(socket_fd, BACKLOG) == -1) {
    close(socket_fd);
    return std::unexpected("Error listening on socket");
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

std::expected<std::vector<std::unique_ptr<SocketHandler>>, std::string>
SocketHandler::accept_connections() const {
  std::vector<std::unique_ptr<SocketHandler>> new_clients;
  while (true) {
    int client_fd =
        accept4(socket_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else {
        return std::unexpected(std::format(
            "error while accepting new connection: {}", c_error_string()));
      }
    }
    new_clients.emplace_back(std::make_unique<SocketHandler>(client_fd));
  }
  return new_clients;
}

std::expected<std::string, std::string> SocketHandler::read_user_input() const {
  std::string buffer;
  buffer.resize(4096);  // add this magic number to config file
  ssize_t n = recv(this->socket_fd, buffer.data(), buffer.size(), 0);
  if (n > 0) {
    buffer.resize(n);
    LOG(std::format("Message recieved: {}", buffer));
    return buffer;
  } else if (n == 0) {
    return std::string("close");
  } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
    return std::unexpected("Nothing to read, returning ");
  } else {
    return std::unexpected(std::format("Read error: {}", c_error_string()));
  }
}

std::expected<void, std::string> SocketHandler::write_to_user(
    std::string_view string_to_send) const {
  LOG(std::format("Message for user: {}", string_to_send.data()));
  if (send(this->socket_fd, string_to_send.data(), string_to_send.size(), 0) ==
      -1) {
    return std::unexpected(
        std::format("Failed to send a message, error: {}", c_error_string()));
  }
  return {};
}
std::expected<void, std::string> SocketHandler::remove_cloexec() {
  int flags = fcntl(this->socket_fd, F_GETFD);
  if (flags == -1) {
    return std::unexpected("Cant get socket flags");
  }
  flags &= ~FD_CLOEXEC;
  if (fcntl(this->socket_fd, F_SETFD, flags) == -1) {
    return std::unexpected("Cant set socket flags");
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

std::expected<void, std::string> EpollHandler::init() {
  epollfd = epoll_create1(EPOLL_CLOEXEC);
  if (epollfd == -1) return std::unexpected("Error creating epoll");
  return {};
}

std::expected<void, std::string> EpollHandler::add_socket(
    SocketHandler* const socket_handler) {
  struct epoll_event event;
  event.data.ptr = static_cast<void*>(socket_handler);
  event.events = EPOLLIN | EPOLLET;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_handler->get_socket(), &event) ==
      -1)
    return std::unexpected(
        std::format("error adding socket to epoll: {}", c_error_string()));
  return {};
}

std::expected<void, std::string> EpollHandler::remove_socket(
    const SocketHandler* const socket_handler) {
  if (epoll_ctl(epollfd, EPOLL_CTL_DEL, socket_handler->get_socket(), NULL) ==
      -1)
    return std::unexpected(
        std::format("error removing socket from epoll: {}", c_error_string()));
  return {};
}

std::expected<std::vector<SocketHandler*>, std::string>
EpollHandler::wait_for_events(size_t max_events) const {
  std::vector<struct epoll_event> events(max_events);
  // struct epoll_event events[10];
  int n = epoll_wait(this->epollfd, events.data(), 10, -1);
  if (n == -1)
    return std::unexpected(
        std::format("error in epoll_wait(): {}", c_error_string()));
  std::vector<SocketHandler*> ready_fds;
  for (int i = 0; i < n; ++i) {
    ready_fds.push_back(static_cast<SocketHandler*>((events[i].data.ptr)));
  }
  return ready_fds;
}
}  // namespace bsm
