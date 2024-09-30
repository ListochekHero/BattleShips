#ifndef SERVER_H
#define SERVER_H

#include <sys/wait.h>

#include <list>
#include <memory>

#include "data_storage.h"
#include "socket_routine.h"

#define MAX_EVENTS 10

namespace BattleShipsMain {
class Application {
 public:
  virtual void run() = 0;

 private:
};
class Server : public Application, ObservableSu8ject {
 public:
  Server(int port);
  void run();
  Observer *find_handler_by_socket(int sock_fd);
  void attach(std::unique_ptr<Observer> observer) override;
  void detach(std::unique_ptr<Observer> observer) override;

 private:
  std::unique_ptr<EpollHandler> epoll_handler;
  std::unique_ptr<std::list<Lobbies>> lobbies;

  std::map<const int, std::unique_ptr<Observer>> observers;

  void notify(int sock_fd) override;
  void handle_new_client_connection(int clientfd);
  void handle_client_request_for_lobby(int client_fd);
  void handle_zombie_pocesses();
  void close_all_sockets_but_one(int sock_fd);
};
class Client : public Application {
 public:
 private:
};
}  // namespace BattleShipsMain
#endif