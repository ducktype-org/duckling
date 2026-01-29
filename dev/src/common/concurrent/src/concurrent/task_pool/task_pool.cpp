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
		execution_completed_cv.wait(lock, [this] { return !is_executing.load(); });
	}

	void TaskPool::addInitialTasks(std::vector<PoolTask> tasks) {
		std::lock_guard lock(pool_mutex);
		for (auto& task: tasks) global_pool.push_back(std::move(task));
		added_tasks.fetch_add(tasks.size());
	}

	void TaskPool::waitExecutionCompletion() {
		std::unique_lock lock(pool_mutex);
		execution_completed_cv.wait(lock, [this] { return not is_executing.load(); });
	}

	void TaskPool::execute() {
		is_executing.store(true);
		std::unique_lock lock(pool_mutex);

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
					tryExecuteTask(pt);
				}
			);
		}

		task_completed_cv.wait(lock, [this] {
			std::cout << base::strConcat(
				"Completed tasks: ", completed_tasks.load(), " / ", added_tasks.load(), "\n"
			);
			return added_tasks.load() <= completed_tasks.load();
		});

		is_executing.store(false);
		execution_completed_cv.notify_all();
	}

	void TaskPool::query(const PoolTask& task) {
		added_tasks.fetch_add(1);
		bool task_done = tryExecuteTask(task);
		if (not task_done) {
			const auto       id = task.id;
			std::unique_lock lock(pool_mutex);
			task_completed_cv.wait(lock, [this, id] { return isTaskDone(id); });
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
			task_completed_cv.notify_all();
			return true;
		}
		if (task_status_map.getCopy(task.id) == TaskStatus::Done) {
			completed_tasks.fetch_add(1);
			task_completed_cv.notify_all();
			return true;
		}

		std::cout << base::strConcat(
			"Worker ",
			static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
			" not completed task ",
			task.id,
			" (in progress)\n"
		);
		// This line, although mabye counter intuitive on the first sight, is correct.
		// It's because we don't count task completion here, but rather the number of added tasks
		// to the pool. Some tasks may be added multiple times, (but only one execution will
		// happen), so we pair the numbers of added tasks (
		completed_tasks.fetch_add(1);
		return false;
	}

	TaskHandle TaskPool::schedule(PoolTask&& task) {
		// This function is the most problematic in terms of using independent queues,
		WorkerID   current_worker = Worker::getCurrentWorkerID();
		const auto task_id        = task.id;

		// This line is not needed, but it avoids scheduling (some) duplicate tasks
		if (task_status_map.contains(task_id)) {
			std::cout << base::strConcat(
				"Worker ",
				static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
				" tried to schedule already scheduled task ",
				task_id,
				"\n"
			);
			// Task is already scheduled or done
			return TaskHandle(*this, task_id);
		}

		// Now becasue we are adding a new task to the pool, we increment the added tasks counter.
		added_tasks.fetch_add(1);

		{
			std::lock_guard lock(pool_mutex);
			auto            free_workers = worker_manager.getFreeWorkers(num_workers);
			// @note It is possible that the free worker would not be free by the time we will
			// schedule a task on it. This would be unfortunate, but not a problem, as the worker
			// manager will just queue the task for later execution.

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
			} else {
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
		}

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
		auto task_opt = tryStealFromWorkerUnlocked(Worker::getCurrentWorkerID(), id);
		if_opt_some(task_opt, task) {
			std::cout << base::strConcat(
				"Worker ",
				static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
				" stole awaited task ",
				id,
				" from its own pool and will try to execute it\n"
			);
			lock.unlock();
			tryExecuteTask(task);
			lock.lock();
		}
		task_completed_cv.wait(lock, [this, id] { return isTaskDone(id); });
	}

	bool TaskPool::isTaskDone(TaskID id) const {
		return task_status_map.contains(id) && task_status_map.getCopy(id) == TaskStatus::Done;
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

	base::Optional<PoolTask> TaskPool::tryStealFromWorkerUnlocked(
		WorkerID worker_id, TaskID task_id
	) {
		auto& worker_pool = worker_pools[static_cast<usize>(worker_id)];
		for (auto it = worker_pool.begin(); it != worker_pool.end(); ++it) {
			if (it->id == task_id) {
				PoolTask task = std::move(*it);
				worker_pool.erase(it);
				return task;
			}
		}

		return std::nullopt;
	}

	void TaskPool::addToGlobalPoolUnlocked(PoolTask&& task) {
		global_pool.push_back(std::move(task));
	}

	void TaskPool::addToWorkerPoolUnlocked(WorkerID worker_id, PoolTask&& task) {
		worker_pools[static_cast<u64>(worker_id)].push_back(std::move(task));
	}

	void TaskPool::onWorkerNoTasks() {
		if (!is_executing.load()) return;

		base::Optional<PoolTask> task_opt;
		std::lock_guard          lock(pool_mutex);

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
		}
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
}  // namespace concurrent
