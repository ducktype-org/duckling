#pragma once

#include <concurrent/worker/task.hpp>

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <random>
#include <thread>

namespace concurrent::worker {
	class Worker;
	using WRef = Ref<Worker>;

	/**
	 * @brief Callback type invoked when a worker has no tasks to execute.
	 */
	using NoTasksCallback = std::function<void(WRef)>;

	/**
	 * @brief Represents a task to be executed by a worker.
	 */
	using Task = std::function<void(WRef)>;

	/**
	 * @brief Represents a single worker in the WorkerManager.
	 * @note All methods are thread-safe.
	 * Worker's inner thread is joined upon destruction.
	 */
	class Worker final {
		friend class WorkerManager;

	public:
		Worker(const Worker&)            = delete;
		Worker(Worker&&)                 = delete;
		Worker& operator=(const Worker&) = delete;
		Worker& operator=(Worker&&)      = delete;

		~Worker();

		/**
		 * @brief Pushes a task to the worker's task queue.
		 * @param task The task to be executed.
		 */
		void scheduleTask(Task&& task);

		/**
		 * @brief Checks if a worker is free.
		 * Free means that the worker is not currently executing any task
		 * and has no tasks in its queue.
		 */
		[[nodiscard]] bool isFree() const;

		/**
		 * @brief Sets the callback to be invoked when there are no tasks.
		 * @param callback The callback function.
		 * @note If a worker is free, it will call the new callback
		 * immediately. This means, that the callback may be called more than once
		 * if the worker has no tasks - once in the method call, and later when the worker
		 * loop checks for tasks. If the first call adds tasks, then the second call will not
		 * happen.
		 */
		void setNoTasksCallback(const NoTasksCallback& callback);

		u64 randomU64() const { return u64(rng()); }

	private:
		Worker(usize seed);

		/**
		 * @brief Starts the worker's main loop in a separate thread.
		 */
		void run();

		/**
		 * @brief The random number generator for the worker.
		 */
		mutable std::mt19937_64 rng;

		/**
		 * @brief Indicates whether the worker is free.
		 */
		std::atomic_bool is_free       = true;
		std::atomic_bool loop_run_flag = true;  /// Controls the main loop of the worker thread.

		NoTasksCallback no_tasks_callback = [](WRef) {};  /// Callback when there are no tasks.

		std::queue<Task> task_queue;

		mutable std::mutex mut;  /// Internal synchronization mutex.
		std::condition_variable
			task_cv;             /// Condition variable to notify the worker thread about new tasks.

		std::jthread real_thread;
	};
}

template<>
struct std::hash<concurrent::worker::WRef> {
	[[nodiscard]] size_t operator()(const concurrent::worker::WRef& worker_ref) const noexcept {
		return std::hash<usize>{}(reinterpret_cast<usize>(worker_ref.get()));
	}
};
