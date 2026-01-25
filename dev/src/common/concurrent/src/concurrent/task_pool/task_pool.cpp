#include "concurrent/worker/worker.hpp"

#include <concurrent/task_pool/task_pool.hpp>

#include "base/except/exceptions.hpp"
#include "base/str/str_utils.hpp"

#include <atomic>
#include <iostream>
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
		std::unique_lock lock(pool_mutex);
		new_task_or_completed_cv.wait(lock, [this] { return !is_executing.load(); });
		
	}

	void TaskPool::addInitialTasks(std::vector<PoolTask> tasks) {
		std::lock_guard lock(pool_mutex);
		for (auto& task: tasks) global_pool.push_back(std::move(task));
		added_tasks.fetch_add(tasks.size());
	}

	void TaskPool::waitExecutionCompletion() {
		std::unique_lock lock(pool_mutex);
		new_task_or_completed_cv.wait(lock, [this] { return !is_executing.load(); });
	}

	void TaskPool::startExecution() {
		is_executing.store(true);
		std::unique_lock lock(pool_mutex);

		// Distribute tasks: first num_workers go to workers, rest to global pool
		auto  worker_ids            = worker_manager.getAllWorkers();
		usize num_distributed_tasks = std::min(global_pool.size(), worker_ids.size());
		std::vector<PoolTask> tasks_to_distribute;

		for (usize i = 0; i < num_distributed_tasks; ++i) {
			tasks_to_distribute.push_back(std::move(global_pool.front()));
			global_pool.pop_front();
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

		new_task_or_completed_cv.wait(lock, [this] {
			std::cout << base::strConcat(
				"Completed tasks: ", completed_tasks.load(), " / ", added_tasks.load(), "\n"
			);
			return added_tasks.load() <= completed_tasks.load();
		});

		is_executing.store(false);
		new_task_or_completed_cv.notify_all();
	}

	void TaskPool::query(const PoolTask& task) {
		added_tasks.fetch_add(1);
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
			auto wd = worker_manager.getWorkerDataFromID(Worker::getCurrentWorkerID());

			task.work(wd);
			{
				std::lock_guard lock(pool_mutex);
				task_status_map.update(task.id, TaskStatus::Done);
				std::cout << base::strConcat(
					"Worker ",
					static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
					" completed task ",
					task.id,
					"\n"
				);
				completed_tasks.fetch_add(1);
			}
			new_task_or_completed_cv.notify_all();
			return true;
		}
		if (task_status_map.getCopy(task.id) == TaskStatus::Done) {
			new_task_or_completed_cv.notify_all();
			completed_tasks.fetch_add(1);
			std::cout << base::strConcat(
				"Worker ",
				static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
				" found task ",
				task.id,
				" already done\n"
			);
			return true;
		}

		std::cout << base::strConcat(
			"Worker ",
			static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
			" not completed task ",
			task.id,
			" (in progress)\n"
		);
		completed_tasks.fetch_add(1);
		return false;
	}

	TaskHandle TaskPool::schedule(PoolTask&& task) {
		// This function is the most problematic in terms of using independent queues,
		WorkerID current_worker = Worker::getCurrentWorkerID();
		added_tasks.fetch_add(1);
        const auto task_id = task.id;

		// This is not needed, as the `tryExecuteTask` uses `maybePut` and would not start
		// executing if the task is already in progress or done.
		// if (task_status_map.contains(task_id)) {
		//     // Task already scheduled or done
		//     return TaskHandle(*this, task_id);
		// }


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

				std::cout << base::strConcat(
					"Worker ",
					static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
					" scheduled task ",
					task_id,
					" to free worker ",
					static_cast<usize>(free_workers.front()),
					"\n"
				);
				return TaskHandle(*this, task_id);
				// We don't add to the pool, as it is scheduled directly
				// and in the pool are only unscheduled and unstarted tasks.
			}

			std::cout << base::strConcat(
				"Worker ",
				static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
				" scheduled task ",
				task_id,
				" to its own pool\n"
			);
			// Add to current worker's pool
			addToWorkerPoolUnlocked(current_worker, std::move(task));
		}

		new_task_or_completed_cv.notify_all();

		return TaskHandle(*this, task_id);
	}

	void TaskPool::await(TaskID id) {
		std::unique_lock lock(pool_mutex);
		std::cout << base::strConcat(
			"Worker ",
			static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
			" awaiting task ",
			id,
			"\n"
		);
		while (not isTaskDone(id)) {
			// Try to do useful work while waiting
			if (auto task_opt = findWorkUnlocked()) {
				std::cout << base::strConcat(
					"Worker ",
					static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
					" found work to do while awaiting task ",
					id,
					" (task ",
					task_opt->id,
					")\n"
				);
				lock.unlock();
				tryExecuteTask(*task_opt);
				lock.lock();
			} else {
                // yield
                // lock.unlock();
                // std::this_thread::yield();
                // lock.lock();
				std::cout << base::strConcat(
					"Worker ",
					static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
					" is waiting on cv for task ",
					id,
					"\n"
				);

				new_task_or_completed_cv.wait(lock, [this, id] {
					return isTaskDone(id) or isWorkAvailableUnlocked();
				});

				std::cout << base::strConcat(
					"Worker ",
					static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
					" woke up while awaiting task ",
					id,
					"\n"
				);
			}
		}
		std::cout << base::strConcat(
			"Worker ",
			static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
			" finished awaiting task ",
			id,
			"\n"
		);
	}

	bool TaskPool::isTaskDone(TaskID id) const {
		std::cout << base::strConcat(
			"Worker ",
			static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
			" checking if task ",
			id,
			" is done\n",
			task_status_map.contains(id) ? " (found in map)" : " (not found in map)",
			task_status_map.contains(id)
				? (task_status_map.getCopy(id) == TaskStatus::Done ? " (status::Done)\n"
		                                                           : " (not Done)\n")
				: "\n"
		);
		return task_status_map.contains(id) && task_status_map.getCopy(id) == TaskStatus::Done;
	}

	base::Optional<PoolTask> TaskPool::findWorkUnlocked() {
		// printQueuesDebugInfo();
		// Then, try doing work from own pool
		if (auto task_opt = tryStealFromWorkerUnlocked(Worker::getCurrentWorkerID())) {
			std::cout << base::strConcat(
				"Worker ",
				static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
				" steals from own pool task ",
				task_opt->id,
				"\n"
			);
			return task_opt;
		}

		// Second, try the global pool
		if (auto task_opt = tryStealFromGlobalUnlocked()) {
			std::cout << base::strConcat(
				"Worker ",
				static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
				" steals from global pool task ",
				task_opt->id,
				"\n"
			);
			return task_opt;
		}

		// Then try other workers' pools
		for (auto worker_id: worker_manager.getAllWorkers()) {
			if (worker_id == Worker::getCurrentWorkerID()) continue;

			if (auto task_opt = tryStealFromWorkerUnlocked(worker_id)) {
				std::cout << base::strConcat(
					"Worker ",
					static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
					" steals from worker ",
					static_cast<usize>(worker_id),
					" task ",
					task_opt->id,
					"\n"
				);
				return task_opt;
			}
		}
		std::cout << base::strConcat(
			"Worker ",
			static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
			" found no work to do\n"
		);

		return {};
	}

	base::Optional<PoolTask> TaskPool::tryStealFromGlobalUnlocked() {
		if (!global_pool.empty()) {
			PoolTask task = std::move(global_pool.front());
			global_pool.pop_front();
			return task;
		}
		return std::nullopt;
	}

	base::Optional<PoolTask> TaskPool::tryStealFromWorkerUnlocked(WorkerID worker_id) {
		auto& worker_pool = worker_pools[static_cast<usize>(worker_id)];
		if (!worker_pool.empty()) {
			PoolTask task = std::move(worker_pool.front());
			worker_pool.pop_front();
			return task;
		}

		return std::nullopt;
	}

	void TaskPool::addToGlobalPoolUnlocked(PoolTask &&task) {
		global_pool.push_back(std::move(task));
	}

	void TaskPool::addToWorkerPoolUnlocked(WorkerID worker_id, PoolTask&& task) {
		worker_pools[static_cast<u64>(worker_id)].push_back(std::move(task));
	}

	void TaskPool::onWorkerNoTasks() {
		// Try to find and execute work from the pool
		// Finding the task if available
        if (!is_executing.load()) {
            return;
        }
		// return;


		base::Optional<PoolTask> task_opt;
        std::lock_guard lock(pool_mutex);

		if (auto global_task_opt = tryStealFromGlobalUnlocked()) {
			std::cout << base::strConcat(
				"Worker ",
				static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
				" found task ",
				global_task_opt->id,
				" in global pool on no-tasks callback\n"
			);
			task_opt = std::move(global_task_opt);
		}

		if (task_opt.empty()) {
			for (auto worker_id: worker_manager.getAllWorkers()) {
				if (auto worker_task_opt = tryStealFromWorkerUnlocked(worker_id)) {
					task_opt = std::move(worker_task_opt);
					std::cout << base::strConcat(
						"Worker ",
						static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
						" found task ",
						task_opt->id,
						" in worker ",
						static_cast<usize>(worker_id),
						" pool on no-tasks callback\n"
					);
					break;
				}
			}
		}

		if (task_opt.has_value()) {
			worker_manager.scheduleTaskOnWorker(
				Worker::getCurrentWorkerID(),
				[this, pt = std::move(task_opt).value()](WDRef) mutable { tryExecuteTask(pt); }
			);
		} else {
			std::cout << base::strConcat(
				"Worker ",
				static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
				" found no tasks on no-tasks callback\n"
			);
		}
		// If no task found, simply return,
		// the worker remains idle until new tasks are scheduled.
	}

	void TaskHandle::await() { pool.await(task_id); }

	void TaskPool::printQueuesDebugInfo() {
		std::lock_guard lock(pool_mutex);
		// print global tasks and local tasks
		std::cout << "Global pool [" << global_pool.size() << "] : ";
		for (usize i = 0; i < global_pool.size(); ++i) std::cout << global_pool[i].id << " ";
		std::cout << "\n";

		for (usize i = 0; i < worker_pools.size(); ++i) {
			std::cout << "Worker " << i << " pool [" << worker_pools[i].size() << "] : ";
			for (usize j = 0; j < worker_pools[i].size(); ++j)
				std::cout << worker_pools[i][j].id << " ";
			std::cout << "\n";
		}
	}

	bool TaskPool::isWorkAvailableUnlocked() {
		if (!global_pool.empty()) return true;

		for (const auto& worker_pool: worker_pools)
			if (!worker_pool.empty()) return true;

		return false;
	}
}  // namespace concurrent
