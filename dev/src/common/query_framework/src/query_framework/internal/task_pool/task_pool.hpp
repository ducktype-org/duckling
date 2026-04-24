#pragma once

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/base/collections/queue.hpp>
#include <concurrent/worker/worker.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <query_framework/internal/query_graph/node_id.hpp>

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <mutex>
#include <type_traits>
#include <vector>

namespace query::internal {
	class TaskPool;

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
		NodeID                   id;
		concurrent::worker::Task work;

		template<typename F>
		requires(std::invocable<std::decay_t<F>&, concurrent::worker::WRef>)
		Task(NodeID id, F&& fn): id(id), work(std::forward<F>(fn)) {}
	};

	/**
	 * @brief Handle returned when scheduling tasks, allows waiting on completion.
	 */
	class TaskHandle final {
	public:
		explicit TaskHandle(TaskPool& pool, NodeID id): pool(pool), task_id(id) {}

		[[nodiscard]] NodeID getID() const { return task_id; }

		void await();

	private:
		TaskPool& pool;
		NodeID    task_id;
	};

	/**
	 * @brief Task pool scheduler that manages task distribution across workers.
	 * @note All public methods are thread-safe.
	 */
	class TaskPool final {
		using WRef = concurrent::worker::WRef;

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
		 * @brief Add the tasks to the pool.
		 */
		void addTask(Task&& tasks);

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
		void await(NodeID id);


		/**
		 * @brief Check if a task is complete.
		 * @param id The task ID to check.
		 *
		 * @return True if the task is done, false otherwise.
		 */
		[[nodiscard]] bool isTaskDone(NodeID id) const;

		/**
		 * @brief Callback invoked when a worker has no tasks.
		 * Attempts to steal work from the pool and schedule it on the
		 * current worker.
		 */
		void onWorkerNoTasks();

		/**
		 * @brief Waits until some worker executed the task.
		 */
		void waitForTask(NodeID id);

		void invalidateTask(NodeID id);

	private:
		/**
		 * @brief Try to steal a task from the global pool.
		 * @return Optional Task if one was available.
		 */
		base::Optional<Task> tryStealFromGlobal();

		/**
		 * @brief Try to steal a task from another worker's pool.
		 * @param worker_ref The ID of the worker to steal from.
		 * @return Optional Task if one was available.
		 */
		base::Optional<Task> tryStealFromWorker(WRef worker_ref);

		/**
		 * @brief Same as above but only steals the task of the given ID by @param task_id.
		 */
		base::Optional<Task> tryStealFromWorker(WRef worker_ref, NodeID task_id);

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
		void addToWorkerPool(WRef worker_ref, Task&& task);

		/**
		 * @brief Add a task to the global pool.
		 * @param task The task to add.
		 */
		void addToGlobalPool(Task&& task);

		/**
		 * @brief Gets the reference of a free worker if available.
		 * There is a similar function in WorkerManager, but here we
		 * set the availability of the worker under our synchronization,
		 * avoiding the missed wake-up problem (a missed schedule in this case).
		 * @note This will set a worker as not free, so the caller should set it back to free if it
		 * fails to schedule a task on it.
		 * @return Optional reference to a free worker.
		 */
		base::Optional<WRef> getFreeWorker();

		/// Reference to the WorkerManager.
		concurrent::worker::WorkerManager& worker_manager;

		/// Number of workers.
		usize num_workers;

		/// Global task pool (shared among all workers).
		concurrent::ConQueue<Task> global_pool;

		/// Per-worker task pools.
		std::vector<concurrent::ConQueue<Task>> worker_pools;

		/// Map from TaskID to TaskStatus (concurrent, lock-free access).
		/// @TODO: #1988 hash map per query id? Or even stronger, lock free data structure.
		concurrent::ConHashMap<NodeID, TaskStatus> task_status_map;
		// concurrent::ConHashMap<NodeID, TaskStatus, std::hash<NodeID>, 4096, 129, 64> task_status_map;

		static constexpr usize              TASK_SHARDS = 113;
		std::array<std::mutex, TASK_SHARDS> task_completed_mutexes;
		/// Condition variable for signaling task completion.
		std::array<std::condition_variable, TASK_SHARDS> task_completed_cv;

		/// Our own worker free (see getFreeWorkerUnlocked() function) for more info.
		std::vector<std::atomic<bool>> is_worker_free;
	};

}  // namespace concurrent
