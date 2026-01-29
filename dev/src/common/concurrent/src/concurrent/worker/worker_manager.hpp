#pragma once

#include <concurrent/module_flags/worker_count.hpp>
#include <concurrent/worker/worker.hpp>

#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

namespace concurrent::worker {
	/**
	 * @brief Manages a fixed number of workers to execute tasks.
	 * @note Destruction of WorkerManager first STOPS and then joins all workers. This means:
	 * - a task that is currently being executed by a worker will fully complete.
	 * - all other scheduled tasks will be discarded.
	 * It might not be a desirable behavior in some scenarios. Consider waiting on a
	 * condition variable or calling `std::this_thread::yield()` in the main thread to ensure all
	 * tasks are completed before destroying the WorkerManager.
	 */
	class WorkerManager final {
		friend void setWorkerCount(u64);

		/**
		 * @brief Constructs a WorkerManager with the specified number of workers.
		 * @param num_workers The number of workers to create.
		 * @note This constructor is private. Use `WorkerManager::get()` to obtain the singleton
		 * instance. It is meant to be called by `setWorkerCount` only.
		 */
		static void setWorkers(usize num_workers);

	public:
		WorkerManager(const WorkerManager&)  = delete;
		void operator=(const WorkerManager&) = delete;

		static WorkerManager& get();

		/**
		 * @brief Returns a vector of all workers managed by the WorkerManager.
		 */
		[[nodiscard]] std::vector<WRef> getAllWorkers() const;

		/**
		 * @brief Returns a vector of free workers.
		 * @param max_count The maximum number of free workers to return.
		 */
		[[nodiscard]] std::vector<WRef> getFreeWorkers(usize max_count) const;

		/**
		 * @brief Schedules a task on a specific worker.
		 * @param worker The worker to schedule the task on.
		 * @param task The task to be executed.
		 */
		void scheduleTaskOnWorker(WRef worker, Task task);

		/**
		 * @brief Schedules a task on any worker, while preferring free workers.
		 * If no free worker is available, the task is scheduled on a random worker.
		 * @param task The task to be executed.
		 */
		void scheduleTaskOnAnyWorker(Task task);

		/**
		 * @brief Checks if a worker is free.
		 * @param worker_id The ID of the worker to check.
		 * @return True if the worker is free, false otherwise.
		 */
		[[nodiscard]] bool isWorkerFree(WRef worker) const;

		/**
		 * @brief Sets a callback to be called when the worker has no tasks to execute.
		 * @param worker The worker.
		 * @param callback The callback function to be called.
		 * @note If a worker is free, it will call the new callback
		 * immediately. This means, that the callback may be called more than once
		 * if the worker has no tasks - once in the method call, and later when the worker
		 * loop checks for tasks. If the first call adds tasks, then the second call will not
		 * happen.
		 */
		void setNoTasksCallback(WRef worker, const NoTasksCallback& callback);

	private:
		WorkerManager() = default;

		/**
		 * @brief Array of workers managed by the WorkerManager.
		 */
		std::vector<Box<Worker>> workers;
	};

}
