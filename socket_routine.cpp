#include "socket_routine.h"

namespace BattleShipsMain {

SocketHandler::SocketHandler(int port) {
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd == -1) throw std::runtime_error("Error creating socket");

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    close(sock_fd);
    throw std::runtime_error("Error binding socket");
  }

  if (listen(sock_fd, BACKLOG) == -1) {
    close(sock_fd);
    throw std::runtime_error("Error listening on socket");
  }

  make_non_blocking();
}

SocketHandler::~SocketHandler() { close(sock_fd); }

int SocketHandler::get_fd() const { return sock_fd; }

void SocketHandler::set_fd(int sock_fd) { this->sock_fd = sock_fd; }

int SocketHandler::accept_connection() {
  int client_fd = accept(sock_fd, nullptr, nullptr);
  if (client_fd < 0) {
    throw std::runtime_error("Error while accepting new connection");
  }
  return client_fd;
}

void SocketHandler::make_non_blocking() {
  int flags = fcntl(sock_fd, F_GETFL, 0);
  if (flags == -1 || fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    throw std::runtime_error("Error making socket non-blocking");
  }
}

EpollHandler::EpollHandler() {
  epollfd = epoll_create1(0);
  if (epollfd == -1) throw std::runtime_error("Error creating epoll");
}

EpollHandler::~EpollHandler() { close(epollfd); }

void EpollHandler::add_socket(int sock_fd) {
  struct epoll_event event;
  event.data.fd = sock_fd;
  event.events = EPOLLIN;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock_fd, &event) == -1) {
    throw std::runtime_error("Error adding socket to epoll");
  }
}

std::vector<int> EpollHandler::wait_for_events(int max_events) {
  std::vector<struct epoll_event> events(max_events);
  int n = epoll_wait(epollfd, events.data(), max_events, -1);

  std::vector<int> ready_fds;
  for (int i = 0; i < n; ++i) {
    ready_fds.push_back(events[i].data.fd);
  }
  return ready_fds;
}

}  // namespace BattleShipsMain