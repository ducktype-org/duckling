#include "worker.hpp"

#include <base/except/exceptions.hpp>

#include <random>

namespace concurrent::worker {
	void Worker::scheduleTask(Task&& task) {
		CORE_ASSERT(loop_run_flag, "Cannot push task to stopped worker");
		{
			std::scoped_lock lock(mut);
			task_queue.push(std::move(task));
			is_free = false;
		}
		task_cv.notify_one();
	}

	[[nodiscard]] bool Worker::isFree() const {
		std::scoped_lock lock(mut);
		return is_free;
	}

	Worker::~Worker() {
		{
			std::scoped_lock lock(mut);
			loop_run_flag = false;
		}
		task_cv.notify_one();
		real_thread.join();
	}

	void Worker::run() {
		real_thread = std::jthread{ [this]() {
			while (loop_run_flag) {
				Task task;
				{
					std::unique_lock lock(mut);

					if (task_queue.empty()) {
						// This is copied as the callback might be changed during its invocation.
						auto copy_no_tasks_callback = no_tasks_callback;
						lock.unlock();
						copy_no_tasks_callback(this);
						lock.lock();
					}

					// This check is needed in case during the callback the worker was stopped.
					if (!loop_run_flag) return;

					while (task_queue.empty()) {
						is_free = true;
						task_cv.wait(lock);
						if (!loop_run_flag) return;
					}
					task = task_queue.front();
					task_queue.pop();
				}
				task(this);
			}
		} };
	}

	void Worker::setNoTasksCallback(const NoTasksCallback& callback) {
		{
			std::unique_lock lock(mut);
			no_tasks_callback = callback;

			if (is_free) {
				lock.unlock();
				// Calling the original `callback`, because `no_tasks_callback` might be changed
				// during the invocation.
				callback(this);
			}
		}
	}

	Worker::Worker(usize seed): rng(seed) {}
}
