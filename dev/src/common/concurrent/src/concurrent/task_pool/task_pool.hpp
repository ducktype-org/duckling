#pragma once

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/worker/worker.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace concurrent::pool {
	class TaskPool;

	/**
	 * @brief TaskID is a unique identifier for tasks in the TaskPool.
	 */
	using TaskID = std::size_t;

	/**
	 * @brief Status of a task in the TaskPool.
	 */
	enum class TaskStatus : uint8_t {
		// NotStarted,  ///< Currently if a task is not in the map, it is not started.
		InProgress,  ///< Task is currently being executed by a worker.
		Done,        ///< Task has completed execution.
	};

	/**
	 * @brief A task with an associated ID for tracking in the pool.
	 */
	struct Task final {
		// @TODO: #1973 integrate with query.
		TaskID       id;
		worker::Task work;

		Task(TaskID id, worker::Task work): id(id), work(std::move(work)) {}
	};

	/**
	 * @brief Handle returned when scheduling tasks, allows waiting on completion.
	 */
	class TaskHandle final {
	public:
		explicit TaskHandle(TaskPool& pool, TaskID id): pool(pool), task_id(id) {}

		[[nodiscard]] TaskID getId() const { return task_id; }

		void await();

	private:
		TaskPool& pool;
		TaskID    task_id;
	};

	/**
	 * @brief Task pool scheduler that manages task distribution across workers.
	 * @note All public methods are thread-safe.
	 */
	class TaskPool final {
	public:
		/**
		 * @brief Constructs a TaskPool.
		 * It uses the WorkerManager singleton to get the workers 
		 * and set the no_tasks_callback for each worker to its onWorkerNoTasks method.
		 */
		explicit TaskPool();

		~TaskPool();

		TaskPool(const TaskPool&)            = delete;
		TaskPool(TaskPool&&)                 = delete;
		TaskPool& operator=(const TaskPool&) = delete;
		TaskPool& operator=(TaskPool&&)      = delete;


		/**
		 * @brief Add the initial set of tasks to the pool.
		 * These tasks will be distributed to workers when execute() is called.
		 * 
		 * For now it may only be called once, it panics if called more than once.
		 */
		void addInitialTasks(std::vector<Task> tasks);

		/**
		 * @brief Start execution of all tasks in the pool.
		 * Distributes initial tasks: one to each worker and the rest to the global pool.
		 * Is non-blocking, returns immediately after scheduling the initial tasks.
		 * @note execute() must be called after addInitialTasks()
		 */
		void execute();

		/**
		 * @brief Wait for all tasks in the pool to complete.
		 * Should be called after execute().
		 */
		void waitExecutionCompletion();

		/**
		 * @brief Query (execute) a task immediately.
		 *
		 * If the task is already being executed by another worker, this will
		 * wait for it to complete.
		 *
		 * @param task The task to execute.
		 *
		 * @note Must be called from a worker thread.
		 */
		void query(const Task& task);

		/**
		 * @brief Schedule a task for later execution.
		 *
		 * Tasks are added to the pool and can be awaited using wait().
		 *
		 * @param task The task to schedule.
		 * @return TaskHandle for scheduled task.
		 *
		 * @note Must be called from a worker thread.
		 */
		TaskHandle schedule(Task&& task);

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
		 *
		 * @return True if the task is done, false otherwise.
		 */
		[[nodiscard]] bool isTaskDone(TaskID id) const;

		/**
		 * @brief Callback invoked when a worker has no tasks.
		 * Attempts to steal work from the pool and shedules it on the 
		 * current worker. Should be called from the worker's no_tasks_callback.
		 */
		void onWorkerNoTasks(worker::WRef current_worker);

	private:
		/**
		 * @brief Try to steal a task from the global pool.
		 * @return Optional Task if one was available.
		 */
		base::Optional<Task> tryStealFromGlobalUnlocked();

		/**
		 * @brief Try to steal a task from another worker's pool.
		 * @param worker_ref The ID of the worker to steal from.
		 * @return Optional Task if one was available.
		 */
		base::Optional<Task> tryStealFromWorkerUnlocked(worker::WRef worker_ref);

		/**
		 * @brief Same as above but only steals the task of the given ID by @param task_id.
		 */
		base::Optional<Task> tryStealFromWorkerUnlocked(worker::WRef worker_ref, TaskID task_id);

		/**
		 * @brief Tries to execute the given task.
		 * If the task is already in progress or done, does nothing.
		 * If the task is not started, executes it.
		 * @param task The task to execute.
		 * @return True if the task has been completed by us or was already
		 * done in the middle of the function.
		 * Otherwise returns false.
		 * 
		 * So if returns true we know for sure that the task is done,
		 * but if returns false then we don't know if the task is done or still in progress.
		 */
		bool tryExecuteTask(const Task& task);

		/**
		 * @brief Add a task to a worker's local pool.
		 * @param worker_ref The worker ref.
		 * @param task The task to add.
		 */
		void addToWorkerPoolUnlocked(worker::WRef worker_ref, Task&& task);

		/**
		 * @brief Add a task to the global pool.
		 * @param task The task to add.
		 */
		void addToGlobalPoolUnlocked(Task&& task);

		/**
		 * @brief Gets the reference of a free worker if available.
		 * There is a similiar function in WorkerManager, but here we
		 * set the availability of the worker under our mutex
		 * avoiding the missed wake up problem (missed schedule problem in this case).
		 */
		base::Optional<worker::WRef> getFreeWorkerUnlocked() const;


		/**
		 * @brief Waits until the `no_tasks_callback` has exited on all workers 
		 * to ensure that the workers are not executing any method of the TaskPool
		 * object to safely destroy it.
		 */
		void flushWorkers();

		/// Reference to the WorkerManager.
		worker::WorkerManager& worker_manager;

		/// Number of workers.
		usize num_workers;

		/// Main mutex protecting pool queues (global_pool, worker_pools, pending_tasks).
		mutable std::mutex pool_mutex;
		/// Global task pool (shared among all workers).
		std::deque<Task> global_pool;

		/// Per-worker task pools.
		base::HashMap<worker::WRef, std::deque<Task>> worker_pools;

		/// Map from TaskID to TaskStatus (concurrent, lock-free access).
		ConHashMap<TaskID, TaskStatus> task_status_map;

		/// Condition variable for signaling task completion.
		std::condition_variable task_completed_cv;

		/// Counter for completed tasks (used in execute()).
		std::atomic<usize> completed_tasks{ 0 };

		/// Total number of tasks (used in execute()).
		std::atomic<usize> added_tasks{ 0 };

		/// Our own worker free (see getFreeWorkerUnlocked() function) for more info.
		base::HashMap<worker::WRef, std::atomic<bool>> is_worker_free_map;

		/// Flag indicating if execution is in progress.
		std::atomic<bool> is_executing{ false };

		/// If addInitialTasks() was called already, to prevent multiple calls.
		bool first_call = true;
	};

}  // namespace concurrent
