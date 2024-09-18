#ifndef INTERFACES_H
#define INTERFACES_H

namespace BattleShipsMain {
class ObserverGeneral {
  virtual void update() = 0;
};
class ObservableSubject {
 public:
  virtual void attach(ObserverGeneral *observer) = 0;
  virtual void detach(ObserverGeneral *observer) = 0;
  virtual void notify() = 0;
}
}  // namespace BattleShipsMain

#endif