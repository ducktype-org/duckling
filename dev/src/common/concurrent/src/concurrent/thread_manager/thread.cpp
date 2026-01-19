#include "thread.hpp"

#include <base/except/exceptions.hpp>

#include <mutex>

void concurrent::Thread::pushTask(Task&& task) {
	CORE_ASSERT(loop_run_flag, "Cannot push task to stopped thread");
	{
		std::lock_guard lock(m);
		task_queue.push(std::move(task));
	}
	cv.notify_one();
}

concurrent::Thread::Thread(ThreadID id):
	  id(id),
	  real_thread([this]() {
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

void concurrent::Thread::stop() {
	{
		std::lock_guard<std::mutex> lock(m);
		loop_run_flag = false;
	}
	cv.notify_one();
	real_thread.join();
}
