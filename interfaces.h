#ifndef INTERFACES_H
#define INTERFACES_H

#include <memory>

namespace BattleShipsMain {
class Observer {
 public:
  virtual void update(int sock_fd) = 0;
  virtual ~Observer() = default;
};
class ObservableSu8ject {
 public:
  virtual void attach(std::unique_ptr<Observer> observer) = 0;
  virtual void detach(std::unique_ptr<Observer> observer) = 0;
  virtual void notify(int sock_fd) = 0;
};
}  // namespace BattleShipsMain

#endif