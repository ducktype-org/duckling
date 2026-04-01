#include "worker.hpp"

#include <base/except/exceptions.hpp>

#include <memory>
#include <random>

namespace concurrent::worker {
	namespace {
		thread_local base::Optional<WRef> current_worker = std::nullopt;
	}

	constinit u64 Worker::next_id = 0;

	void Worker::scheduleTask(Task&& task) {
		const bool can_schedule = loop_run_flag.load(std::memory_order_relaxed);
		CORE_ASSERT(can_schedule, "Cannot push task to stopped worker");
		{
			std::scoped_lock lock(mut);
			is_free = false;
			task_queue.push(std::move(task));
		}
		task_cv.notify_one();
	}

	bool Worker::scheduleTaskIfFree(Task&& task) {
		{
			std::scoped_lock lock(mut);
			if (!is_free) return false;
			is_free = false;
			task_queue.push(std::move(task));
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
						auto no_tasks_callback_ptr = no_tasks_callback;
						lock.unlock();
						(*no_tasks_callback_ptr)(this);
						lock.lock();
					}

					// This check is needed in case during the callback the worker was stopped.
					if (!loop_run_flag) return;

					while (task_queue.empty()) {
						is_free = true;
						task_cv.wait(lock);
						if (!loop_run_flag) return;
					}
					task = std::move(task_queue.front());
					task_queue.pop();
				}
				task(this);
			}
		} };
	}

	void Worker::setNoTasksCallback(NoTasksCallback&& callback) {
		auto callback_ptr = std::make_shared<NoTasksCallback>(std::move(callback));
		setNoTasksCallback(std::move(callback_ptr));
	}

	void Worker::setNoTasksCallback(std::shared_ptr<NoTasksCallback> callback) {
		{
			std::unique_lock lock(mut);
			no_tasks_callback = callback;

			if (!is_free) return;

			// Set is free to false, since worker will be calling no tasks callback
			// The workers are only free in waiting on the condition variable
			is_free = false;

			// Schedule this callback as a task, if the worker was free we have a guarantee
			// that the callback will be called first
			task_queue.emplace([callback](WRef ref) { (*callback)(ref); });

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

	bool Worker::isCurrentThreadWorker() { return current_worker.has_value(); }

	u64 Worker::getID() const { return id; }
}
