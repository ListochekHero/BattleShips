#ifndef INTERFACES_H
#define INTERFACES_H

namespace BattleShipsMain {
class Observer {
 public:
  virtual void update(int sock_fd) = 0;
};
class ObservableSu8ject {
 public:
  virtual void attach(int sock_fd, Observer *observer) = 0;
  virtual void detach(Observer *observer) = 0;
  virtual void notify(int sock_fd) = 0;
};

class Application {
 public:
  virtual void run() = 0;

 private:
};
}  // namespace BattleShipsMain

#endif