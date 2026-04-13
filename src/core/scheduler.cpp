#include "scheduler.h"

#include <iostream>
#include <ostream>

namespace bsm {

void Scheduler::push_task(std::string task_tag,
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
  }
  return task;
}

bool Scheduler::add_executor(std::string tag, TaskExecutor executor) {
  auto [it, inserted] = tasks_executors_.try_emplace(tag, executor);
  return inserted;
}

auto& Scheduler::get_executor_by_tag(std::string task_tag) {
  return tasks_executors_[task_tag];
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

} // namespace bsm
