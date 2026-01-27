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
	public:
		/**
		 * @brief Constructs a WorkerManager with getWorkerCount() workers.
		 */
		WorkerManager();

		/**
		 * @brief Returns a vector of all worker IDs managed by the WorkerManager.
		 */
		std::vector<WorkerID> getAllWorkers();

		/**
		 * @brief Returns a vector of free worker IDs.
		 * @param max_count The maximum number of free worker IDs to return.
		 */
		std::vector<WorkerID> getFreeWorkers(usize max_count);

		/**
		 * @brief Schedules a task on a specific worker.
		 * @param worker_id The ID of the worker to schedule the task on.
		 * @param task The task to be executed.
		 */
		void scheduleTaskOnWorker(WorkerID worker_id, Task task);

		/**
		 * @brief Schedules a task on any free worker.
		 * If no free worker is available, the task is scheduled on a random worker.
		 * @param task The task to be executed.
		 */
		void scheduleTaskOnAnyFreeWorker(Task&& task);

		/**
		 * @brief Checks if a worker is free.
		 * @param worker_id The ID of the worker to check.
		 * @return True if the worker is free, false otherwise.
		 */
		[[nodiscard]] bool isWorkerFree(WorkerID worker_id) const;

		/**
		 * @brief Sets a callback to be called when the worker has no tasks to execute.
		 * @param worker_id The ID of the worker.
		 * @param callback The callback function to be called.
		 */
		void setNoTasksCallback(WorkerID worker_id, NoTasksCallback callback);

	private:
		/**
		 * @brief The number of workers managed by the WorkerManager.
		 */
		const usize num_workers;

		/**
		 * @brief Array of workers managed by the WorkerManager.
		 */
		std::vector<Box<Worker>> workers;
	};

}
