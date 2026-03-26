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
		/**
		 * Initializes or resets the WorkerManager state.
		 * This is separated from the constructor to allow resetting the state in unit tests.
		 * See also: testPrivateAccessReloadState.
		 */
		void setup(usize num_workers);

	public:
		/**
		 * @brief Tests access to private reload state for unit testing.
		 */
		static void testPrivateAccessReloadState();

		WorkerManager(const WorkerManager&)  = delete;
		void operator=(const WorkerManager&) = delete;
		WorkerManager(WorkerManager&&)       = delete;
		void operator=(WorkerManager&&)      = delete;

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
		 * @brief Schedules a task on any worker, while preferring free workers.
		 * If no free worker is available, the task is scheduled on a random worker.
		 * @param task The task to be executed.
		 * @return The reference of the worker the task was scheduled on.
		 */
		WRef scheduleTaskOnAnyWorker(const Task& task);

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
		 * @warning The lifetime of the variables used in the callback must be guaranteed to be
		 * longer than the lifetime of a callback inside a worker. This means that you need to make
		 * sure to reset the callback before the destruction of the variables
		 */
		void setNoTasksCallback(WRef worker, const NoTasksCallback& callback);

		/**
		 * @brief Same as above, but sets the same callback for all workers sequentially.
		 */
		void setNoTasksCallback(const NoTasksCallback& callback) {
			for (auto& worker: getAllWorkers()) worker->setNoTasksCallback(callback);
		}

	private:
		WorkerManager();

		/**
		 * @brief Array of workers managed by the WorkerManager.
		 */
		std::vector<Box<Worker>> workers;
	};

}
