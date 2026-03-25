#include "worker.hpp"

#include <base/except/exceptions.hpp>

#include <random>

namespace concurrent::worker {
	namespace {
		thread_local base::Optional<WRef> current_worker = std::nullopt;
	}

	void Worker::scheduleTask(const Task& task) {
		CORE_ASSERT(loop_run_flag, "Cannot push task to stopped worker");
		{
			std::scoped_lock lock(mut);
			is_free = false;
			task_queue.push(task);
		}
		task_cv.notify_one();
	}

	bool Worker::scheduleTaskIfFree(const Task& task) {
		{
			std::scoped_lock lock(mut);
			if (!is_free) return false;
			is_free = false;
			task_queue.push(task);
		}
		task_cv.notify_one();
		return true;
	}

	bool Worker::isFree() const { return is_free; }

	bool Worker::internalHasTasks() const {
		std::scoped_lock lock(mut);
		return !task_queue.empty();
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
			current_worker.emplace(this);
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

			// This is copied as the no_tasks_callback might be changed during its invocation.
			// Also the lifetime of the callback is not guaranteed to be longer than the invocation.
			auto copy_no_tasks_callback = callback;

			if (!is_free) return;

			// Set is free to false, since worker will be calling no tasks callback
			// The workers are only free in waiting on the condition variable
			is_free = false;

			// Schedule this callback as a task, if the worker was free we have a guarantee
			// that the callback will be called first
			task_queue.emplace([copy_no_tasks_callback = std::move(copy_no_tasks_callback)](WRef ref
			                   ) { copy_no_tasks_callback(ref); });

			// wake up the worker to call the scheduled callback
		}
		task_cv.notify_one();
	}

	Worker::Worker(usize seed): rng(seed) {}

	WRef Worker::getCurrentWorker() {
		if (!current_worker.has_value())
			CORE_PANIC("Accessing the thread-local current worker reference that is empty");
		return current_worker.value();
	}
}
