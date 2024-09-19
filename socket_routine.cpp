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

void SocketHandler::accept_connection() {
  int client_fd = accept(sock_fd, nullptr, nullptr);
  if (client_fd < 0) {
    throw std::runtime_error("Error while accepting new connection");
  }
  if (connection_callback) {
    connection_callback(client_fd);
  }
}

void SocketHandler::update(int sock_fd) {
  if (this->sock_fd == sock_fd) {
    this->accept_connection();
  }
}
void SocketHandler::set_connection_callback(ConnectionCallback callback) {
  connection_callback = callback;
}
void SocketHandler::make_non_blocking() {
  int flags = fcntl(sock_fd, F_GETFL, 0);
  if (flags == -1 || fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    throw std::runtime_error("Error making socket non-blocking");
  }
}

void SocketIOHandler::read_from_socket() {
  char buffer[BUFF_SIZE];
  int n = read(this->get_fd(), buffer, BUFF_SIZE);
  if (n < 0) {
    // TODO
  }
  safe_buffer.assign(buffer);
}

EpollHandler::EpollHandler() {
  epollfd = epoll_create1(0);
  if (epollfd == -1) throw std::runtime_error("Error creating epoll");
}

EpollHandler::~EpollHandler() { close(epollfd); }

void EpollHandler::add_socket(int sock_fd) {
  struct epoll_event event;
  event.data.fd = sock_fd;
  event.events = EPOLLIN | EPOLLET;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock_fd, &event) == -1) {
    throw std::runtime_error("Error adding socket to epoll");
  }
}

void EpollHandler::attach(int sock_fd, Observer *observer) {
  observers[sock_fd] = observer;
};
void EpollHandler::detach(Observer *observer) {
  observers.erase(std::remove(observers.begin(), observers.end(), observer),
                  observers.end());
};

void EpollHandler::wait_for_events(int max_events) {
  std::vector<struct epoll_event> events(max_events);
  int n = epoll_wait(epollfd, events.data(), max_events, -1);

  for (int i = 0; i < n; ++i) {
    int sock_fd = events[i].data.fd;
    if (observers.find(sock_fd) != observers.end()) notify(sock_fd);
  }
}
void EpollHandler::notify(int sock_fd) {
  if (observers[sock_fd]) {
    observers[sock_fd]->update(sock_fd);
  }
}
}  // namespace BattleShipsMain