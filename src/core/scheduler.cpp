#include "scheduler.h"

#include "core/atomic_queue.h"

#include <utility>

namespace bsm {

void Scheduler::push_task(task_tag_e tag,
                          std::unique_ptr<TaskContext> context) {
  push_task_semaphore_.acquire();
  Task task{.tag = tag, .context = std::move(context)};
  tasks_queue_.mmanager_push(
      std::move(task)); // We ignore returned bool because this is invariant
  // and and should never happen
  pop_task_semaphore_.release();
}

auto Scheduler::add_task_executor(task_tag_e tag, TaskExecutor executor)
    -> std::optional<Error> {
  auto [iter, inserted] = task_executors_.try_emplace(tag, executor);
  if (!inserted) {
    return Error{
        .backtrace =
            {"Unable to add executor for task: failed to emplace into map"},
    };
  }
  return std::nullopt;
}

void Scheduler::run_workers() {
  for (auto& thread : thread_pool_) {
    thread = std::thread([this]() -> void { worker_loop(); });
  }
}

auto Scheduler::get_co_handle_by_tag(task_tag_e tag)
    -> std::coroutine_handle<> {
  return co_handles_map_[tag];
}

auto Scheduler::try_get_task() -> std::optional<Task> {
  pop_task_semaphore_.acquire();
  auto task{tasks_queue_.mmanager_try_pop()};
  if (task) {
    push_task_semaphore_.release();
  } else {
    pop_task_semaphore_.release();
  }
  return task;
}

auto Scheduler::get_executor_by_tag(task_tag_e tag) -> auto& {
  return task_executors_[tag];
}

void Scheduler::worker_loop() {
  while (true) {
    auto ready_task{try_get_task()};
    if (ready_task) {
      auto& task_executor{get_executor_by_tag(ready_task->tag)};
      task_executor(std::move(ready_task->context));
    }
  }
}

auto Scheduler::coroutine_loop() -> bsm_co_handle { // NOLINT
  while (true) {
    available_task_tags_.mmanager_wait_for_data();
    auto task_tag{available_task_tags_.mmanager_try_pop()};
    if (task_tag.has_value()) {
      auto handle{get_co_handle_by_tag(*task_tag)};
      handle.resume();
    }
  }
}

} // namespace bsm
