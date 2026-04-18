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
  bsm_co_handle get_return_object() {
    return bsm_co_handle::from_promise(*this);
  }
  std::suspend_always initial_suspend() noexcept { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
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
