#include "concurrent/worker/worker.hpp"

#include <concurrent/task_pool/task_pool.hpp>

#include "base/except/exceptions.hpp"

#include <atomic>
#include <mutex>
#include <thread>

namespace concurrent {

	TaskPool::TaskPool(WorkerManager& worker_manager):
		  worker_manager(worker_manager),
		  num_workers(worker_manager.getAllWorkers().size()) {
		// Initialize per-worker pools
		worker_pools.resize(num_workers);
	}

	TaskPool::~TaskPool() {
		// Ensure all tasks are completed before destruction
		// wait on completion_cv if needed
		if (is_executing.load()) {
			std::unique_lock lock(pool_mutex);
			new_task_or_completed_cv.wait(lock, [this] { return !is_executing.load(); });
		}
	}

	void TaskPool::addInitialTasks(std::vector<PoolTask> tasks) {
		std::lock_guard lock(pool_mutex);
		for (auto& task: tasks) global_pool.push(std::move(task));
	}

	void TaskPool::startExecution() {
		is_executing.store(true);
		completed_tasks.store(0);
		std::unique_lock lock(pool_mutex);

		// Distribute tasks: first num_workers go to workers, rest to global pool
		auto  worker_ids            = worker_manager.getAllWorkers();
		usize num_distributed_tasks = std::min(global_pool.size(), worker_ids.size());
		std::vector<PoolTask> tasks_to_distribute;

		for (usize i = 0; i < num_distributed_tasks; ++i) {
			tasks_to_distribute.push_back(std::move(global_pool.front()));
			global_pool.pop();

			task_status_map.put(tasks_to_distribute.back().id, TaskStatus::InProgress);
		}

		for (usize i = 0; i < num_distributed_tasks; ++i) {
			auto& task = tasks_to_distribute[i];
			worker_manager.scheduleTaskOnWorker(
				worker_ids[i],
				[this, pt = std::move(task)](WDRef) mutable {
					bool result = tryExecuteTask(pt);
					CORE_ASSERT(result, "First task should always start immediately.");
				}
			);
		}

		// Wait for all tasks to complete
		while (completed_tasks.load() < total_tasks.load()) new_task_or_completed_cv.wait(lock);

		is_executing.store(false);
		new_task_or_completed_cv.notify_all();
	}

	void TaskPool::query(const PoolTask &task) {
		bool task_done = tryExecuteTask(task);
		if (not task_done) {
			// Another worker is executing this task, wait for it to complete
			await(task.id);
		}
	}

	bool TaskPool::tryExecuteTask(const PoolTask& task) {
		// @note This can also be achieved via setting to `NotStarted` and then using
		// compareAndExchange(expected=NotStarted, desired=InProgress) and this is strongly
		// preferred by the LLM models (but using `maybePut` has the same semantics but on adding
		// instead of comparing).

		auto change_status_result = task_status_map.maybePut(task.id, TaskStatus::InProgress);
		if (change_status_result.toOpt().has_value()) {
			// The key was inserted by us, we can execute the task
			total_tasks.fetch_add(1, std::memory_order_acquire);
			auto wd = worker_manager.getWorkerDataFromID(Worker::getCurrentWorkerID());

			task.work(wd);

			task_status_map.put(task.id, TaskStatus::Done);
			completed_tasks.fetch_add(1, std::memory_order_release);
			new_task_or_completed_cv.notify_all();

			return true;
		}

		return false;
	}

	TaskHandle TaskPool::schedule(PoolTask&& task) {
		// This function is the most problematic in terms of using independent queues,
		WorkerID current_worker = Worker::getCurrentWorkerID();


		{
			std::lock_guard lock(pool_mutex);
			auto            free_workers = worker_manager.getFreeWorkers(num_workers);
			// Here we want the free worker to remain free until we schedule task on it
			if (not free_workers.empty()) {
				// Schedule on a free worker
				worker_manager.scheduleTaskOnWorker(
					free_workers.front(),
					[this, pt = std::move(task)](WDRef) mutable { tryExecuteTask(pt); }
				);

				return TaskHandle(*this, task.id);
				// We don't add to the pool, as it is scheduled directly
				// and in the pool are only unscheduled and unstarted tasks.
			}

			// Add to current worker's pool
			addToWorkerPool(lock, current_worker, std::move(task));
		}

		new_task_or_completed_cv.notify_one();

		return TaskHandle(*this, task.id);
	}

	void TaskPool::await(TaskID id) {
		while (not isTaskDone(id)) {
			// Try to do useful work while waiting
			if (not tryDoWork()) {
				// No work available, wait for events
				std::unique_lock lock(pool_mutex);
				new_task_or_completed_cv.wait(lock);
				// or just yield
				// std::this_thread::yield();
			}
		}
	}

	bool TaskPool::isTaskDone(TaskID id) const {
		return task_status_map.contains(id) && task_status_map.getCopy(id) == TaskStatus::Done;
	}

	bool TaskPool::tryDoWork() {
		// Then, try doing work from own pool
		if (auto task_opt = tryStealFromWorker(Worker::getCurrentWorkerID()))
			return tryExecuteTask(*task_opt);

		// Second, try the global pool
		if (auto task_opt = tryStealFromGlobal()) return tryExecuteTask(*task_opt);

		// Then try other workers' pools
		for (auto worker_id: worker_manager.getAllWorkers()) {
			if (worker_id == Worker::getCurrentWorkerID()) continue;

			if (auto task_opt = tryStealFromWorker(worker_id)) return tryExecuteTask(*task_opt);
		}

		return false;
	}

	base::Optional<PoolTask> TaskPool::tryStealFromGlobal() {
		std::lock_guard lock(pool_mutex);
		if (!global_pool.empty()) {
			PoolTask task = std::move(global_pool.front());
			global_pool.pop();
			return task;
		}
		return std::nullopt;
	}

	base::Optional<PoolTask> TaskPool::tryStealFromWorker(WorkerID exclude_worker_id) {
		std::lock_guard lock(pool_mutex);

		// Try to steal from other workers' pools
		for (usize i = 0; i < worker_pools.size(); ++i) {
			if (WorkerID(i) == exclude_worker_id) continue;

			if (!worker_pools[i].empty()) {
				// Steal from the back (opposite end from where owner pushes)
				PoolTask task = std::move(worker_pools[i].back());
				worker_pools[i].pop();
				return task;
			}
		}

		return std::nullopt;
	}

	void TaskPool::addToGlobalPool(std::lock_guard<std::mutex>&, PoolTask task) {
		global_pool.push(std::move(task));
	}

	void TaskPool::addToWorkerPool(std::lock_guard<std::mutex>&, WorkerID worker_id, PoolTask task) {
		worker_pools[static_cast<u64>(worker_id)].push(std::move(task));
	}

	void TaskPool::onWorkerNoTasks() {
		// Try to find and execute work from the pool
		// Finding the task if available
		base::Optional<PoolTask> task_opt;
		if (auto global_task_opt = tryStealFromGlobal()) task_opt = std::move(global_task_opt);

		if (task_opt.empty()) {
			for (auto worker_id: worker_manager.getAllWorkers()) {
				if (worker_id == Worker::getCurrentWorkerID()) continue;

				if (auto worker_task_opt = tryStealFromWorker(worker_id))
					task_opt = std::move(worker_task_opt);
			}
		}

		if (task_opt.has_value()) {
			worker_manager.scheduleTaskOnWorker(
				Worker::getCurrentWorkerID(),
				[this, pt = std::move(task_opt).value()](WDRef) mutable { tryExecuteTask(pt); }
			);
		}
        // If no task found, simply return,
        // the worker remains idle until new tasks are scheduled.
	}

	void TaskHandle::await() { pool.await(task_id); }
}  // namespace concurrent
