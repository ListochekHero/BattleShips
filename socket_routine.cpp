#include "socket_routine.h"

namespace bsm {

SocketHandler::SocketHandler(int socket_fd)
    : socket_fd{socket_fd}, listening_socket{false} {}

SocketHandler::~SocketHandler() { close_socket(); }

SocketHandler::SocketHandler(SocketHandler&& sock_hndl)
    : socket_fd{std::exchange(sock_hndl.socket_fd, -1)},
      listening_socket{std::exchange(sock_hndl.listening_socket, false)} {}

SocketHandler& SocketHandler::operator=(SocketHandler&& sock_hndl) {
  if (this != &sock_hndl) {
    this->close_socket();
    socket_fd = std::exchange(sock_hndl.socket_fd, -1);
    listening_socket = std::exchange(sock_hndl.listening_socket, false);
  }
  return *this;
}
std::expected<void, std::string> SocketHandler::setup_listenter(int port) {
  if (socket_fd) return std::unexpected("Socket is already exist");

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
  listening_socket = true;
}

int SocketHandler::get_socket() const { return socket_fd; }

void SocketHandler::swap(SocketHandler& left_sh, SocketHandler& r_sh) {
  // зробити свап (?)
}

bool SocketHandler::is_listening() const { return this->listening_socket; }

void SocketHandler::close_socket() {
  if (!socket_fd) {
    close(socket_fd);
  }
}

std::expected<std::vector<SocketHandler>, std::string>
SocketHandler::accept_connections() {
  std::vector<SocketHandler> new_clients;
  while (true) {
    int client_fd =
        accept4(socket_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else {
        return std::unexpected("Error while accepting new connection");
      }
    }
    new_clients.emplace_back(client_fd);
  }
  return new_clients;
}

std::expected<std::string, std::string> SocketHandler::read_user_input() {
  std::string buffer;
  buffer.resize(4096);  // add this magic number to config file
  ssize_t n = recv(this->socket_fd, buffer.data(), buffer.size(), 0);
  if (n > 0) {
    buffer.resize(n);
    return buffer;
  } else if (n == 0) {
    return std::unexpected("Connection closed");
  } else {
    return std::unexpected("Read error: " + std::to_string(errno));
  }
}

std::expected<void, std::string> EpollHandler::init() {
  epollfd = epoll_create1(EPOLL_CLOEXEC);
  if (epollfd == -1) return std::unexpected("Error creating epoll");
}

EpollHandler::~EpollHandler() { close(epollfd); }

std::expected<void, std::string> EpollHandler::add_socket(
    SocketHandler& socket_handler) {
  struct epoll_event event;
  event.data.ptr = static_cast<void*>(&socket_handler);
  event.events = EPOLLIN | EPOLLET;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_handler.get_socket(), &event) ==
      -1)
    return std::unexpected("Error adding socket to epoll");
  return {};
}

std::expected<std::vector<SocketHandler*>, std::string>
EpollHandler::wait_for_events(int max_events) {
  std::vector<struct epoll_event> events(max_events);
  int n = epoll_wait(epollfd, events.data(), max_events, -1);
  if (n == -1) return std::unexpected("Error in epoll_wait");
  std::vector<SocketHandler*> ready_fds;
  for (int i = 0; i < n; ++i) {
    ready_fds.push_back(static_cast<SocketHandler*>((events[i].data.ptr)));
  }
  return ready_fds;
}
}  // namespace bsm
