#include "scheduler.h"

#include "core/atomic_queue.h"

#include <iostream>
#include <utility>

namespace bsm {

void Scheduler::init() {
  co_handlers_map_.try_emplace(task_tag_e::SCHEDULER, co_run());
}

void Scheduler::push_task(task_tag_e task_tag,
                          std::unique_ptr<TaskContext> context) {
  push_c_semaphore_.acquire();
  Task* task = new Task(task_tag, std::move(context));
  tasks_queue_.push(task);
  pop_c_semaphore_.release();
}

auto Scheduler::try_get_task() -> Task* {
  pop_c_semaphore_.acquire();
  Task* task = tasks_queue_.try_pop();
  if (task != nullptr) {
    push_c_semaphore_.release();
  } else {
    pop_c_semaphore_.release();
  }
  return task;
}

auto Scheduler::add_executor(task_tag_e tag, TaskExecutor executor) -> bool {
  auto [iter, inserted] = task_executors_.try_emplace(tag, executor);
  return inserted;
}

auto Scheduler::get_executor_by_tag(task_tag_e task_tag) -> auto& {
  return task_executors_[task_tag];
}

auto Scheduler::get_co_by_tag(task_tag_e task_tag) -> std::coroutine_handle<> {
  return co_handlers_map_[task_tag];
}

void Scheduler::worker_loop() {
  while (true) {
    std::unique_ptr<Task> task_to_exe{try_get_task()};
    if (task_to_exe) {
      auto& executor{get_executor_by_tag(task_to_exe->task_tag)};
      executor(std::move(task_to_exe->context));
      std::cout << "Task complited!" << '\n';
    }
  }
}

void Scheduler::run_workers() {
  for (auto& thread : thread_pool_) {
    thread = std::thread([this]() -> void { worker_loop(); });
  }
}

auto Scheduler::co_run() -> bsm_co_handle {
  while (true) {
    available_task_tags_.wait_for_data();
    co_await *this;
  }
}

} // namespace bsm
