#include "concurrent/worker/worker.hpp"

#include <concurrent/task_pool/task_pool.hpp>

#include "base/except/exceptions.hpp"
#include "base/str/str_utils.hpp"

#include <atomic>
#include <iostream>
#include <mutex>

namespace concurrent::pool {

	TaskPool::TaskPool(worker::WorkerManager& worker_manager):
		  worker_manager(worker_manager),
		  num_workers(worker_manager.getAllWorkers().size()) {
		// Initialize per-worker pools
		for (auto worker: worker_manager.getAllWorkers())
			worker_pools.emplace(worker, std::deque<Task>());
	}

	TaskPool::~TaskPool() {
		// Ensure all tasks are completed before destruction
		// wait on completion_cv if needed
		waitExecutionCompletion();
	}

	void TaskPool::addInitialTasks(std::vector<Task> tasks) {
		std::lock_guard lock(pool_mutex);
		for (auto& task: tasks) global_pool.push_back(std::move(task));
		added_tasks.fetch_add(tasks.size());
	}

	void TaskPool::waitExecutionCompletion() {
		std::unique_lock lock(pool_mutex);
		task_completed_cv.wait(lock, [this] {
			return added_tasks.load() <= completed_tasks.load();
		});
		is_executing.store(false);
	}

	void TaskPool::execute() {
		is_executing.store(true);
		std::unique_lock lock(pool_mutex);

		auto              worker_refs           = worker_manager.getAllWorkers();
		usize             n_tasks_to_distribute = std::min(global_pool.size(), worker_refs.size());
		std::vector<Task> tasks_to_distribute;

		for (usize i = 0; i < n_tasks_to_distribute; ++i) {
			tasks_to_distribute.push_back(std::move(global_pool.front()));
			global_pool.pop_front();
		}

		for (usize i = 0; i < n_tasks_to_distribute; ++i) {
			auto& task = tasks_to_distribute[i];
			worker_refs[i]->scheduleTask([this, pt = std::move(task)](worker::WRef) mutable {
				tryExecuteTask(pt);
			});
		}
	}

	void TaskPool::query(const Task& task) {
		added_tasks.fetch_add(1);
		bool task_done = tryExecuteTask(task);
		if (not task_done) {
			const auto       id = task.id;
			std::unique_lock lock(pool_mutex);
			task_completed_cv.wait(lock, [this, id] { return isTaskDone(id); });
		}
	}

	bool TaskPool::tryExecuteTask(const Task& task) {
		// @note This can also be achieved via setting to `NotStarted` and then using
		// compareAndExchange(expected=NotStarted, desired=InProgress) and this is strongly
		// preferred by the LLM models (but using `maybePut` has the same semantics but on adding
		// instead of comparing).

		auto change_status_result = task_status_map.maybePut(task.id, TaskStatus::InProgress);

		if (change_status_result.toOpt().has_value()) {
			// The key was inserted by us, we can execute the task
			auto wd = worker::Worker::getCurrentWorker();

			task.work(wd);
			{
				std::lock_guard lock(pool_mutex);

				task_status_map.update(task.id, TaskStatus::Done);
				completed_tasks.fetch_add(1);
			}
			task_completed_cv.notify_all();
			return true;
		}

		// This line, although mabye counter intuitive on the first sight, is correct.
		// It's because we don't count task completion here, but rather the number of added tasks
		// to the pool. Some tasks may be added multiple times, (but only one execution will
		// happen), so we pair the numbers of added tasks with the number of tasks we taken out of
		// the pool.
		{
			std::lock_guard lock(pool_mutex);
			completed_tasks.fetch_add(1);
		}
		task_completed_cv.notify_all();
		return false;
	}

	TaskHandle TaskPool::schedule(Task&& task) {
		// This function is the most problematic in terms of using independent queues,
		worker::WRef current_worker = worker::Worker::getCurrentWorker();
		const auto   task_id        = task.id;

		// This line is not needed, but it sometimes avoids scheduling duplicate tasks
		if (task_status_map.contains(task_id)) return TaskHandle(*this, task_id);

		// Now becasue we are adding a new task to the pool, we increment the added tasks counter.
		added_tasks.fetch_add(1);

		{
			std::lock_guard lock(pool_mutex);
			auto            free_workers = worker_manager.getFreeWorkers(1);

			// @note It is possible that the free worker would not be free by the time we will
			// schedule a task on it. This would be unfortunate, but not a problem, as the worker
			// manager will just queue the task for later execution.

			if (not free_workers.empty()) {
				// Schedule on a free worker
				free_workers.front()->scheduleTask([this, pt = std::move(task)](worker::WRef
				                                   ) mutable { tryExecuteTask(pt); });
				return TaskHandle(*this, task_id);

				// We don't add it to the pool, as it is scheduled directly
				// and in the pool are only unscheduled and unstarted tasks.
			} else {
				// Add to current worker's pool
				addToWorkerPoolUnlocked(current_worker, std::move(task));
			}
		}

		return TaskHandle(*this, task_id);
	}

	void TaskPool::await(TaskID id) {
		std::unique_lock lock(pool_mutex);
		auto task_opt = tryStealFromWorkerUnlocked(worker::Worker::getCurrentWorker(), id);
		if_opt_some(task_opt, task) {
			lock.unlock();
			tryExecuteTask(task);
			lock.lock();
		}
		task_completed_cv.wait(lock, [this, id] { return isTaskDone(id); });
	}

	bool TaskPool::isTaskDone(TaskID id) const {
		return task_status_map.contains(id) && task_status_map.getCopy(id) == TaskStatus::Done;
	}

	base::Optional<Task> TaskPool::tryStealFromGlobalUnlocked() {
		if (!global_pool.empty()) {
			Task task = std::move(global_pool.front());
			global_pool.pop_front();
			return task;
		}
		return std::nullopt;
	}

	base::Optional<Task> TaskPool::tryStealFromWorkerUnlocked(worker::WRef worker_ref) {
		auto& worker_pool = worker_pools[worker_ref];
		if (!worker_pool.empty()) {
			Task task = std::move(worker_pool.front());
			worker_pool.pop_front();
			return task;
		}

		return std::nullopt;
	}

	base::Optional<Task> TaskPool::tryStealFromWorkerUnlocked(
		worker::WRef worker_ref, TaskID task_id
	) {
		auto& worker_pool = worker_pools[worker_ref];
		for (auto it = worker_pool.begin(); it != worker_pool.end(); ++it) {
			if (it->id == task_id) {
				Task task = std::move(*it);
				worker_pool.erase(it);
				return task;
			}
		}

		return std::nullopt;
	}

	void TaskPool::addToGlobalPoolUnlocked(Task&& task) { global_pool.push_back(std::move(task)); }

	void TaskPool::addToWorkerPoolUnlocked(worker::WRef worker_ref, Task&& task) {
		worker_pools[worker_ref].push_back(std::move(task));
	}

	void TaskPool::onWorkerNoTasks() {
		if (!is_executing.load()) return;

		base::Optional<Task> task_opt;
		std::lock_guard      lock(pool_mutex);

		if (auto global_task_opt = tryStealFromGlobalUnlocked())
			task_opt = std::move(global_task_opt);

		if (task_opt.empty()) {
			for (auto worker_ref: worker_manager.getAllWorkers()) {
				if (auto worker_task_opt = tryStealFromWorkerUnlocked(worker_ref)) {
					task_opt = std::move(worker_task_opt);
					break;
				}
			}
		}

		if (task_opt.has_value()) {
			auto current_worker = worker::Worker::getCurrentWorker();
			current_worker->scheduleTask([this, pt = std::move(task_opt).value()](worker::WRef
			                             ) mutable { tryExecuteTask(pt); });
		}
	}

	void TaskHandle::await() { pool.await(task_id); }
}  // namespace concurrent::pool
