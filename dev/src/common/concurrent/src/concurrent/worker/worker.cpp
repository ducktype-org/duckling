#include "worker.hpp"

#include <base/except/exceptions.hpp>

namespace concurrent::worker {
	void Worker::pushTask(Task&& task) {
		CORE_ASSERT(loop_run_flag, "Cannot push task to stopped worker");
		{
			std::scoped_lock lock(mut);
			task_queue.push(std::move(task));
		}
		task_cv.notify_one();
	}

	Worker::Worker(WDRef worker_data): worker_data(worker_data), real_thread() {}

	[[nodiscard]] bool Worker::isFree() const {
		std::scoped_lock lock(mut);
		return !is_occupied && task_queue.empty();
	}

	[[nodiscard]] WorkerID Worker::getId() const { return worker_data->getID(); }

	Worker::~Worker() {
		{
			std::scoped_lock lock(mut);
			loop_run_flag = false;
		}
		task_cv.notify_one();
		real_thread.join();
	}

	[[nodiscard]] WDRef Worker::getWorkerData() const { return worker_data; }

	void Worker::run() {
		real_thread = std::jthread{ [this]() {
			while (loop_run_flag) {
				Task task;
				{
					std::unique_lock lock(mut);

					if (task_queue.empty()) {
						lock.unlock();
						no_tasks_callback(worker_data);
						lock.lock();
					}

					while (task_queue.empty()) {
						is_occupied = false;
						task_cv.wait(lock);
						if (!loop_run_flag) return;
					}
					is_occupied = true;
					task        = task_queue.front();
					task_queue.pop();
				}
				task(worker_data);
			}
		} };
	}

	void Worker::setNoTasksCallback(NoTasksCallback callback) {
		no_tasks_callback = std::move(callback);
	}
}
