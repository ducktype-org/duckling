#pragma once

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
		Worker()                         = delete;
		Worker(const Worker&)            = delete;
		Worker(Worker&&)                 = delete;
		Worker& operator=(const Worker&) = delete;
		Worker& operator=(Worker&&)      = delete;

		~Worker();

		/**
		 * @brief Pushes a task to the worker's task queue.
		 * @param task The task to be executed.
		 */
		void scheduleTask(const Task& task);

		/**
		 * @brief Pushes a task to the worker's task queue only if the worker is free.
		 * @param task The task to be executed.
		 * @return True if the task was pushed, false otherwise.
		 */
		bool scheduleTaskIfFree(const Task& task);

		/**
		 * @brief Checks if a worker is free.
		 * Free means that the worker is not currently executing any task
		 * and has no tasks in its queue.
		 */
		[[nodiscard]] bool isFree() const;

		/**
		 * @brief Checks if the worker has tasks in its queue.
		 * Used specifically when you need it and when you are sure
		 * that there are no race conditions on scheduling the tasks.
		 *
		 * Used in the TaskPool to prevent the situation when we exit
		 * the worker's no_tasks_callback without new schedules
		 * and before setting the worker as free we want to schedule something.
		 */
		[[nodiscard]] bool internalHasTasks() const;

		/**
		 * @brief Sets the callback to be invoked when there are no tasks.
		 * @param callback The callback function.
		 * @note If a worker is free, it will call the new callback
		 * immediately. This means, that the callback may be called more than once
		 * if the worker has no tasks - once in the method call, and later when the worker
		 * loop checks for tasks. If the first call adds tasks, then the second call will not
		 * happen.
		 * @warning The lifetime of the variables used in the callback must be guaranteed to be
		 * longer than the lifetime of a callback inside a worker. This means that you need to make
		 * sure to reset the callback before the destruction of the variables
		 */
		void setNoTasksCallback(const NoTasksCallback& callback);

		/**
		 * @brief Gets the reference of the current worker
		 * Panics if called from a non-worker thread.
		 */
		static WRef getCurrentWorker();

	private:
		Worker(usize seed);

		/**
		 * @brief Starts the worker's main loop in a separate thread.
		 */
		void run();

		/**
		 * @brief Generates a random u64 using the worker's RNG.
		 * @note This is NOT thread-safe, as std::mt19937_64 is not thread-safe. It should only be
		 * called from the worker's main loop or with external synchronization. The randomness is
		 * not guaranteed to be high-quality, but it is sufficient for load balancing tasks.
		 */
		u64 randomU64() const { return u64(rng()); }

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
