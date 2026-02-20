#include "worker.hpp"

#include <base/except/exceptions.hpp>

#include <concurrent/base/profiling/wait_stats.hpp>

#include <chrono>
#include <random>

namespace concurrent::worker {
	namespace {
		thread_local base::Optional<WRef> current_worker = std::nullopt;
	}

	void Worker::scheduleTask(const Task& task) {
		CORE_ASSERT(loop_run_flag, "Cannot push task to stopped worker");
		{
			auto wm_t0 = std::chrono::steady_clock::now();
			std::scoped_lock lock(mut);
			auto wm_dt = std::chrono::steady_clock::now() - wm_t0;
			concurrent::g_wait_stats.worker_mut_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(wm_dt).count(), std::memory_order_relaxed);
			concurrent::g_wait_stats.worker_mut_count.fetch_add(1, std::memory_order_relaxed);
			is_free = false;
			task_queue.push(task);
		}
		task_cv.notify_one();
	}

	bool Worker::scheduleTaskIfFree(const Task& task) {
		{
			auto wm_t0 = std::chrono::steady_clock::now();
			std::scoped_lock lock(mut);
			auto wm_dt = std::chrono::steady_clock::now() - wm_t0;
			concurrent::g_wait_stats.worker_mut_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(wm_dt).count(), std::memory_order_relaxed);
			concurrent::g_wait_stats.worker_mut_count.fetch_add(1, std::memory_order_relaxed);
			if (!is_free) return false;
			is_free = false;
			task_queue.push(task);
		}
		task_cv.notify_one();
		return true;
	}

	bool Worker::isFree() const { return is_free; }

	bool Worker::internalHasTasks() const {
		auto wm_t0 = std::chrono::steady_clock::now();
		std::scoped_lock lock(mut);
		auto wm_dt = std::chrono::steady_clock::now() - wm_t0;
		concurrent::g_wait_stats.worker_mut_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(wm_dt).count(), std::memory_order_relaxed);
		concurrent::g_wait_stats.worker_mut_count.fetch_add(1, std::memory_order_relaxed);
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
					auto wm_t0 = std::chrono::steady_clock::now();
					std::unique_lock lock(mut);
					auto wm_dt = std::chrono::steady_clock::now() - wm_t0;
					concurrent::g_wait_stats.worker_mut_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(wm_dt).count(), std::memory_order_relaxed);
					concurrent::g_wait_stats.worker_mut_count.fetch_add(1, std::memory_order_relaxed);

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
						auto t0 = std::chrono::steady_clock::now();
						task_cv.wait(lock);
						auto dt = std::chrono::steady_clock::now() - t0;
						concurrent::g_wait_stats.worker_idle_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(dt).count(), std::memory_order_relaxed);
						concurrent::g_wait_stats.worker_idle_count.fetch_add(1, std::memory_order_relaxed);
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
			auto wm_t0 = std::chrono::steady_clock::now();
			std::unique_lock lock(mut);
			auto wm_dt = std::chrono::steady_clock::now() - wm_t0;
			concurrent::g_wait_stats.worker_mut_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(wm_dt).count(), std::memory_order_relaxed);
			concurrent::g_wait_stats.worker_mut_count.fetch_add(1, std::memory_order_relaxed);
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
}
