#ifndef SCOPE_GUARD_H
#define SCOPE_GUARD_H

#include <utility>

namespace bsm {

template <typename F> class scope_guard {
public:
  explicit scope_guard(F&& f) : f_(f), active_(true) {};
  scope_guard(const scope_guard&) = delete;
  auto operator=(const scope_guard&) -> scope_guard& = delete;
  scope_guard(scope_guard&&) = delete;
  auto operator=(scope_guard&&) -> scope_guard& = delete;
  ~scope_guard() {
    if (active_) {
      f_();
    }
  }
  void dismiss() { active_ = false; }

private:
  F f_;
  bool active_{};
};

template <typename F> auto make_scope_guard(F&& f) -> scope_guard<F> {
  return scope_guard<F>(std::forward<F>(f));
}

} // namespace bsm
#endif
