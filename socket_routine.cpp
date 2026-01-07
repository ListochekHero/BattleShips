#include "socket_routine.h"

namespace bsm {
SocketHandler::SocketHandler(int socket_fd)
    : socket_fd{socket_fd}, socket_type_v{socket_type_e::CLIENT} {
  LOG("SocketHandler::Constructor(int socket)");
}

SocketHandler::~SocketHandler() {
  LOG("SocketHandler::Destructor()");
  close_socket();
}

SocketHandler::SocketHandler(SocketHandler&& sock_hndl)
    : socket_fd{std::exchange(sock_hndl.socket_fd, -1)},
      socket_type_v{
          std::exchange(sock_hndl.socket_type_v, socket_type_e::CLIENT)} {
  LOG("SocketHandler::Constructor(&&SocketHandler)");
}

SocketHandler& SocketHandler::operator=(SocketHandler&& sock_hndl) {
  LOG("SocketHandler::operator=(&&SocketHandler)");
  if (this != &sock_hndl) {
    this->close_socket();
    socket_fd = std::exchange(sock_hndl.socket_fd, -1);
    socket_type_v =
        std::exchange(sock_hndl.socket_type_v, socket_type_e::CLIENT);
  }
  return *this;
}
std::expected<void, std::string> SocketHandler::setup_listenter(int port) {
  LOG("SocketHandler::setup_listenter()");
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

const int SocketHandler::get_socket() const {
  LOG("SocketHandler::get_socket()");
  return socket_fd;
}

void SocketHandler::swap(SocketHandler& left_sh, SocketHandler& r_sh) {
  // зробити свап (?)
}

socket_type_e SocketHandler::socket_type() const {
  LOG("SocketHandler::socket_type()");
  return this->socket_type_v;
}

void SocketHandler::close_socket() {
  LOG("SocketHandler::close_socket()");
  if (!socket_fd) {
    close(socket_fd);
  }
}

std::expected<std::vector<std::unique_ptr<SocketHandler>>, std::string>
SocketHandler::accept_connections() const {
  LOG("SocketHandler::accept_connections()");
  std::vector<std::unique_ptr<SocketHandler>> new_clients;
  while (true) {
    int client_fd =
        accept4(socket_fd, nullptr, nullptr, SOCK_NONBLOCK);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else {
        return std::unexpected("Error while accepting new connection");
      }
    }
    new_clients.emplace_back(std::make_unique<SocketHandler>(client_fd));
  }
  return new_clients;
}

std::expected<std::string, std::string> SocketHandler::read_user_input() const {
  LOG("SocketHandler::read_user_input()");
  std::string buffer;
  // print_socket_flags(this->socket_fd);
  // fcntl(this->socket_fd, F_SETFL, O_NONBLOCK);
  // print_socket_flags(this->socket_fd);

  buffer.resize(4096);  // add this magic number to config file
  ssize_t n = recv(this->socket_fd, buffer.data(), buffer.size(), 0);
  if (n > 0) {
    buffer.resize(n);
    LOG(buffer);
    return buffer;
  } else if (n == 0) {
    return std::string("close");
  } else {
    return std::unexpected("Read error: " + std::to_string(errno));
  }
}

std::expected<void, std::string> SocketHandler::write_to_user(
    std::string_view string_to_send) const {
  LOG("SocketHandler::write_to_user()");
  LOG(string_to_send.data());
  if (send(this->socket_fd, string_to_send.data(), string_to_send.size(), 0) ==
      -1) {
    int err = errno;
    char buff[256];
    LOG("Failed to send a message:");
    LOG(std::strerror(errno));
    LOG("Failed to send a message:");
    LOG(strerror_r(errno, buff, sizeof(buff)));
    return std::unexpected("Failed to send a message");
  }
  LOG("Send was successful");
  return {};
}
std::expected<void, std::string> EpollHandler::init() {
  LOG("EpollHandler::init()");
  epollfd = epoll_create1(EPOLL_CLOEXEC);
  if (epollfd == -1) return std::unexpected("Error creating epoll");
  return {};
}

EpollHandler::~EpollHandler() {
  LOG("EpollHandler::Destructor()");
  close(epollfd);
}

std::expected<void, std::string> EpollHandler::add_socket(
    SocketHandler* const socket_handler) {
  LOG("EpollHandler::add_socket()");
  struct epoll_event event;
  event.data.ptr = static_cast<void*>(socket_handler);
  event.events = EPOLLIN | EPOLLET;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_handler->get_socket(), &event) ==
      -1)
    return std::unexpected("Error adding socket to epoll");
  return {};
}

std::expected<void, std::string> EpollHandler::remove_socket(
    const SocketHandler* const socket_handler) {
  LOG("EpollHandler::remove_socket()");
  if (epoll_ctl(epollfd, EPOLL_CTL_DEL, socket_handler->get_socket(), NULL) ==
      -1)
    return std::unexpected("Error adding socket to epoll");
  return {};
}

std::expected<std::vector<SocketHandler*>, std::string>
EpollHandler::wait_for_events(int max_events) const {
  LOG("EpollHandler::wait_for_events()");
  // std::vector<struct epoll_event> events(max_events);
  struct epoll_event events[10];
  int n = epoll_wait(this->epollfd, events, 10, -1);
  if (n == -1) return std::unexpected("Error in epoll_wait");
  std::vector<SocketHandler*> ready_fds;
  for (int i = 0; i < n; ++i) {
    ready_fds.push_back(static_cast<SocketHandler*>((events[i].data.ptr)));
  }
  return ready_fds;
}
}  // namespace bsm
