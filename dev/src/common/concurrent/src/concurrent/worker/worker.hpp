#pragma once

#include <concurrent/worker/task.hpp>
#include <concurrent/worker/worker_data.hpp>

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/ints.hpp>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace concurrent::worker {
	using NoTasksCallback = std::function<void(WDRef)>;

	/**
	 * @brief Represents a single worker in the WorkerManager.
	 * @note All methods are thread-safe.
	 * Worker's inner thread is joined upon destruction.
	 */
	class Worker final {
		friend class WorkerManager;

	public:
		Worker()                         = delete;
		Worker(const Worker&)            = delete;
		Worker(Worker&&)                 = delete;
		Worker& operator=(const Worker&) = delete;
		Worker& operator=(Worker&&)      = delete;

		~Worker();

		/**
		 * @brief Starts the worker's main loop in a separate thread.
		 */
		void run();

		/**
		 * @brief Pushes a task to the worker's task queue.
		 * @param task The task to be executed.
		 */
		void pushTask(Task&& task);

		/**
		 * @brief Checks if a worker is free.
		 * Free means that the worker is not currently executing any task
		 * and has no tasks in its queue.
		 */
		[[nodiscard]] bool isFree() const;

		/**
		 * @brief Returns the WDRef to the worker's WorkerData.
		 */
		[[nodiscard]] WDRef getWorkerData() const;

		/**
		 * @brief Returns the ID of the worker.
		 */
		[[nodiscard]] WorkerID getId() const;

		/**
		 * @brief Sets the callback to be invoked when there are no tasks.
		 */
		void setNoTasksCallback(NoTasksCallback callback);

	private:
		Worker(WDRef worker_data);

		std::atomic_bool is_occupied
			= false;  /// Indicates whether the worker is currently executing a task.
		std::atomic_bool loop_run_flag = true;  /// Controls the main loop of the worker thread.

		NoTasksCallback no_tasks_callback{};    /// Callback when there are no tasks.

		const WDRef worker_data;

		std::queue<Task> task_queue;

		mutable std::mutex mut;  /// Internal synchronization mutex.
		std::condition_variable
			task_cv;             /// Condition variable to notify the worker thread of new tasks.

		std::jthread real_thread;
	};
}
