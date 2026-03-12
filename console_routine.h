#ifndef CONSOLE_ROUTINE_H
#define CONSOLE_ROUTINE_H

#include <functional>
#include <string>

namespace bsm {
class ConsoleHandler {
public:
  void run();
  using InputHandler = std::function<void(std::string)>;
  void set_input_handler(InputHandler h);

private:
  InputHandler on_input_callback;
};

} // namespace bsm

#endif
