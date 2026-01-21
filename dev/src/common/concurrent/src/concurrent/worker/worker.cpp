#include "worker.hpp"

#include <base/except/exceptions.hpp>

void concurrent::Worker::pushTask(Task&& task) {
	CORE_ASSERT(loop_run_flag, "Cannot push task to stopped worker");
	{
		std::lock_guard lock(m);
		task_queue.push(std::move(task));
	}
	cv.notify_one();
}

concurrent::Worker::Worker(WDRef worker_data, NoTasksCallback no_tasks_callback):
	  no_tasks_callback(std::move(no_tasks_callback)),
	  worker_data(worker_data),
	  real_thread() {}

[[nodiscard]] bool concurrent::Worker::isFree() const {
	std::lock_guard lock(m);
	return !is_occupied.load() && task_queue.empty();
}

[[nodiscard]] concurrent::WorkerID concurrent::Worker::getId() const {
	return worker_data->getID();
}

concurrent::Worker::~Worker() {
	{
		std::lock_guard<std::mutex> lock(m);
		loop_run_flag = false;
	}
	cv.notify_one();
	real_thread.join();
}

[[nodiscard]] concurrent::WDRef concurrent::Worker::getWorkerData() const { return worker_data; }

void concurrent::Worker::run() {
	real_thread = std::jthread{ [this]() {
		while (loop_run_flag) {
			Task task;
			{
				std::unique_lock lock(m);

				if (task_queue.empty()) {
					lock.unlock();
					this->no_tasks_callback(this->worker_data);
					lock.lock();
				}

				while (task_queue.empty()) {
					is_occupied = false;
					cv.wait(lock);
					if (!loop_run_flag) return;
				}
				is_occupied = true;
				task        = task_queue.front();
				task_queue.pop();
			}
			task(this->worker_data);
		}
	} };
}
