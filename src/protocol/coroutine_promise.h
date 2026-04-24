#ifndef COROUTINE_PROMISE_H
#define COROUTINE_PROMISE_H

#include <coroutine>

namespace bsm {

class Application;
class Client;
class Scheduler;

struct bsm_promise;
using bsm_co_handle = std::coroutine_handle<bsm_promise>;

struct bsm_promise {
  auto get_return_object() -> bsm_co_handle {
    return bsm_co_handle::from_promise(*this);
  }
  static auto initial_suspend() noexcept -> std::suspend_always { return {}; }
  static auto final_suspend() noexcept -> std::suspend_always { return {}; }
  void unhandled_exception() {}
};
} // namespace bsm

template <>
struct std::coroutine_traits<bsm::bsm_co_handle, bsm::Application&> {
  using promise_type = bsm::bsm_promise;
};

template <> struct std::coroutine_traits<bsm::bsm_co_handle, bsm::Scheduler&> {
  using promise_type = bsm::bsm_promise;
};

template <> struct std::coroutine_traits<bsm::bsm_co_handle, bsm::Client&> {
  using promise_type = bsm::bsm_promise;
};

#endif
