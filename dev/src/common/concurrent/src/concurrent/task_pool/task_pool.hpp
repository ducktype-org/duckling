#pragma once

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/worker/task.hpp>
#include <concurrent/worker/worker_data.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace concurrent {
	class TaskPool;

	/**
	 * @brief TaskID is a std::size_t because it should be a result of hashing a NodeID
	 */
	using TaskID = std::size_t;

	/**
	 * @brief Status of a task in the TaskPool.
	 */
	enum class TaskStatus : uint8_t {
		// NotStarted,  ///< Task has been added but not yet picked up by a worker.
		InProgress,  ///< Task is currently being executed by a worker.
		Done,        ///< Task has completed execution.
	};

	/**
	 * @brief A task with an associated ID for tracking in the pool.
	 */
	struct PoolTask {
		TaskID id;
		Task   work;

		PoolTask(TaskID id, Task work): id(id), work(std::move(work)) {}
	};

	/**
	 * @brief Handle returned when scheduling tasks, allows waiting on completion.
	 */
	class TaskHandle {
	public:
		explicit TaskHandle(TaskPool& pool, TaskID id): pool(pool), task_id(id) {}

		[[nodiscard]] TaskID getId() const { return task_id; }

		void await();

	private:
		TaskPool& pool;
		TaskID    task_id;
	};

	/**
	 * @brief A group of task handles that can be awaited together.
	 */
	class TaskGroup {
	public:
		TaskGroup() = default;

		void add(TaskHandle handle) { handles.push_back(handle); }

		[[nodiscard]] const std::vector<TaskHandle>& getHandles() const { return handles; }

		[[nodiscard]] bool empty() const { return handles.empty(); }

		[[nodiscard]] usize size() const { return handles.size(); }

	private:
		std::vector<TaskHandle> handles;
	};

	/**
	 * @brief Task pool scheduler that manages task distribution across workers.
	 * @note All public methods are thread-safe.
	 */
	class TaskPool {
	public:
		/**
		 * @brief Constructs a TaskPool with the given WorkerManager.
		 * @param worker_manager Reference to the WorkerManager that provides workers.
		 */
		explicit TaskPool(WorkerManager& worker_manager);

		~TaskPool();

		TaskPool(const TaskPool&)            = delete;
		TaskPool(TaskPool&&)                 = delete;
		TaskPool& operator=(const TaskPool&) = delete;
		TaskPool& operator=(TaskPool&&)      = delete;


		void addInitialTasks(std::vector<PoolTask> tasks);

		/**
		 * @brief Start execution of all tasks in the pool.
		 * Distributes initial tasks: one to each worker, rest to global pool.
		 * Blocks until all tasks are completed.
		 */
		void startExecution();

		/**
		 * @brief Query (execute) a task immediately.
		 *
		 * If the task is already being executed by another worker, this will
		 * wait for it to complete while doing other useful work.
		 *
		 * @param work The work function to execute.
		 * @param wd The worker data reference of the calling worker.
		 * @return TaskID of the executed task.
		 *
		 * @note Must be called from a worker thread.
		 */
		void query(const PoolTask &task);

		/**
		 * @brief Schedule a task for later execution.
		 *
		 * Tasks are added to the pool and can be awaited using wait().
		 *
		 * @param work Work function to schedule.
		 * @param wd The worker data reference of the calling worker.
		 * @return TaskHandle for scheduled task.
		 *
		 * @note Must be called from a worker thread.
		 */
		TaskHandle schedule(PoolTask&& task);

		/**
		 * @brief Wait for a task to complete.
		 *
		 * @param handle The task handle to wait for.
		 * @param wd The worker data reference of the calling worker.
		 *
		 * @note Must be called from a worker thread.
		 */
		void await(TaskID id);


		/**
		 * @brief Check if a task is complete.
		 * @param id The task ID to check.
		 * @return True if the task is done, false otherwise.
		 */
		[[nodiscard]] bool isTaskDone(TaskID id) const;

	private:
		/**
		 * @brief Try to steal a task from the global pool.
		 * @return Optional PoolTask if one was available.
		 */
		base::Optional<PoolTask> tryStealFromGlobal();

		/**
		 * @brief Try to steal a task from another worker's pool.
		 * @param exclude_worker_id Worker ID to exclude from stealing.
		 * @return Optional PoolTask if one was available.
		 */
		base::Optional<PoolTask> tryStealFromWorker(WorkerID exclude_worker_id);

		/**
		 * @brief Try to find and execute any available work.
		 * @param wd The worker data reference of the calling worker.
		 * @return True if work was found and executed, false otherwise.
		 */
		bool tryDoWork();

		/**
		 * @brief Tries to execute the given task.
         * If the task is already in progress or done, does nothing and returns false.
         * If the task is not started, marks it as in progress, executes it and returns true.
		 * @param task The task to execute.
		 */
		bool tryExecuteTask(const PoolTask& task);

		/**
		 * @brief Add a task to a worker's local pool.
		 * @param worker_id The worker ID.
		 * @param task The task to add.
		 */
		void addToWorkerPool(std::lock_guard<std::mutex>&, WorkerID worker_id, PoolTask task);

		/**
		 * @brief Add a task to the global pool.
		 * @param task The task to add.
		 */
		void addToGlobalPool(std::lock_guard<std::mutex>&, PoolTask task);

		/**
		 * @brief Callback invoked when a worker has no tasks.
		 * Attempts to steal work from the pool and schedules it using the WorkerManager.
		 */
		void onWorkerNoTasks();

		/// Reference to the WorkerManager.
		WorkerManager& worker_manager;

		/// Number of workers.
		usize num_workers;

		/// Main mutex protecting pool queues (global_pool, worker_pools, pending_tasks).
		mutable std::mutex pool_mutex;
		/// Global task pool (shared among all workers).
		std::queue<PoolTask> global_pool;
		/// Per-worker task pools.
		std::vector<std::queue<PoolTask>> worker_pools;


		/// Map from TaskID to TaskStatus (concurrent, lock-free access).
		ConHashMap<TaskID, TaskStatus> task_status_map;

		/// Condition variable for signaling task completion or new task arrived.
		std::condition_variable new_task_or_completed_cv;

		/// Counter for completed tasks (used in execute()).
		std::atomic<usize> completed_tasks{ 0 };

		/// Total number of tasks (used in execute()).
		std::atomic<usize> total_tasks{ 0 };

		/// Flag indicating if execution is in progress.
		std::atomic<bool> is_executing{ false };
	};

}  // namespace concurrent
