#include "worker.hpp"

#include <base/except/exceptions.hpp>
#include <timer/timer.hpp>

#include <random>
#include <iostream>

namespace concurrent::worker {

	struct WaitTimePrinter final {
			WaitTimePrinter(std::string_view name_): name(name_) {}
			
			timer::AtomicDuration wait_time;
			std::string_view name;

			~WaitTimePrinter() {
				std::cerr << "TaskPool " << name << ": ";
				timer::printAs(std::cerr, wait_time.toDuration(), timer::TimeUnit::Milliseconds);
				std::cerr << "\n";
			}
		};


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
				static WaitTimePrinter wait_time_printer("Worker between tasks");
				timer::TimeMeasurement tm;
				tm.startMeasurement();

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

				tm.endMeasurement();
				wait_time_printer.wait_time.add(tm.duration());

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

	WRef Worker::getCurrentWorker() {
		if (!current_worker.has_value())
			CORE_PANIC("Accessing the thread-local current worker reference that is empty");
		return current_worker.value();
	}

	bool Worker::isWorkerThread() {
		return current_worker.has_value();
	}
}
