#include "scheduler.h"

#include <iostream>
#include <ostream>

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

Task* Scheduler::try_get_task() {
  pop_c_semaphore_.acquire();
  Task* task = tasks_queue_.pop();
  if (task) {
    push_c_semaphore_.release();
  }else{
    pop_c_semaphore_.release();
  }
  return task;
}

bool Scheduler::add_executor(task_tag_e tag, TaskExecutor executor) {
  auto [it, inserted] = tasks_executors_.try_emplace(tag, executor);
  return inserted;
}

auto& Scheduler::get_executor_by_tag(task_tag_e task_tag) {
  return task_executors_[task_tag];
}

std::coroutine_handle<> Scheduler::get_co_by_tag(task_tag_e task_tag) {
  return co_handlers_map_[task_tag];
}

void Scheduler::worker_loop() {
  while (true) {
    std::unique_ptr<Task> task_to_exe{try_get_task()};
    if (task_to_exe) {
      auto& executor{get_executor_by_tag(task_to_exe->task_tag)};
      executor(std::move(task_to_exe->context));
      std::cout << "Task complited!" << std::endl;
    }
  }
}

void Scheduler::run() {
  for (auto& thread : thread_pool_) {
    thread = std::thread([this]() { return worker_loop(); });
  }
}

bsm_co_handle Scheduler::co_run() {
  while (true) {
    available_task_tags_.wait_for_data();
    co_await *this;
  }
}

} // namespace bsm
