#include "scheduler.h"

#include "core/atomic_queue.h"

#include <iostream>
#include <utility>

namespace bsm {

auto Scheduler::init() -> std::optional<Error> {
  auto [iter, inserted]{
      co_handles_map_.try_emplace(task_tag_e::SCHEDULER, coroutine_loop()),
  };
  if (!inserted) {
    return Error{
        .backtrace =
            {"Unable to init Scheduler: failed to emplace main coroutine"},
    };
  }
  return std::nullopt;
}

void Scheduler::push_task(task_tag_e tag,
                          std::unique_ptr<TaskContext> context) {
  push_task_semaphore_.acquire();
  Task* task = new Task(tag, std::move(context));
  tasks_queue_.push(task); // We ignore returned bool because this is invariant
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

auto Scheduler::try_get_task() -> Task* {
  pop_task_semaphore_.acquire();
  Task* task = tasks_queue_.try_pop();
  if (task != nullptr) {
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
    std::unique_ptr<Task> ready_task{try_get_task()};
    if (ready_task) {
      auto& task_executor{get_executor_by_tag(ready_task->tag)};
      task_executor(std::move(ready_task->context));
    }
  }
}

auto Scheduler::coroutine_loop() -> bsm_co_handle { // NOLINT
  while (true) {
    available_task_tags_.wait_for_data();
    co_await yield_to_application(); // NOLINT
  }
}

} // namespace bsm
