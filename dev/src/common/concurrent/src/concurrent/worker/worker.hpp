#pragma once

#include <concurrent/worker/task.hpp>
#include <concurrent/worker/worker_data.hpp>

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/ints.hpp>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace concurrent {
	using NoTasksCallback = std::function<void(WDRef)>;

	/**
	 * @brief Represents a single worker in the WorkerManager.
	 * @note All methods are thread-safe.
	 * Worker's inner thread is joined upon destruction.
	 */
	class Worker final {
	public:
		Worker(WDRef worker_data, NoTasksCallback no_tasks_callback);
		~Worker();

		Worker(const Worker&)            = delete;
		Worker(Worker&&)                 = delete;
		Worker& operator=(const Worker&) = delete;
		Worker& operator=(Worker&&)      = delete;

		void run();

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

	private:
		std::atomic_bool is_occupied
			= false;  /// Indicates whether the worker is currently executing a task.
		std::atomic_bool loop_run_flag = true;  /// Controls the main loop of the worker thread.

		NoTasksCallback no_tasks_callback;      /// Callback when there are no tasks.

		const WDRef worker_data;

		std::queue<Task> task_queue;

		mutable std::mutex      m;
		std::condition_variable cv;

		std::jthread real_thread;
	};
}
