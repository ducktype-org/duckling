#include "thread.hpp"

#include <mutex>

void concurrent::Thread::pushTask(Task&& task) {
	{
		std::lock_guard lock(m);
		task_queue.push(std::move(task));
	}
	cv.notify_one();
}

concurrent::Thread::Thread(ThreadID id):
	  id(id),
	  worker_thread([this]() {
		  while (loop_run_flag) {
			  Task task{};
			  {
				  std::unique_lock lock(m);
				  while (task_queue.empty()) {
					  is_occupied = false;
					  cv.wait(lock);
					  if (!loop_run_flag) return;
				  }
				  is_occupied = true;
				  task        = task_queue.front();
				  task_queue.pop();
			  }
			  task();
		  }
	  }) {}

[[nodiscard]] bool concurrent::Thread::isFree() const {
	std::lock_guard lock(m);
	return !is_occupied.load() && task_queue.empty();
}

[[nodiscard]] concurrent::ThreadID concurrent::Thread::getId() const { return id; }

concurrent::Thread::~Thread() { stop(); }
