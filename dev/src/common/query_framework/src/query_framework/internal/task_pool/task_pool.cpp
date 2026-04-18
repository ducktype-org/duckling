#include "task_pool.hpp"

#include <concurrent/worker/worker.hpp>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <atomic>
#include <iostream>
#include <mutex>

namespace query::internal {

	TaskPool::TaskPool():
		  worker_manager(concurrent::worker::WorkerManager::get()),
		  num_workers(worker_manager.getAllWorkers().size()) {
		// Initialize per-worker pools
		for (auto worker: worker_manager.getAllWorkers())
			worker_pools.put(worker, std::deque<Task>());

		for (auto worker: worker_manager.getAllWorkers()) is_worker_free_map.put(worker, true);

		// @TODO: #2039 this links query task execution with workers manager logic.
		worker_manager.setNoTasksCallback([this](auto wref) { onWorkerNoTasks(wref); });
	}

	TaskPool::~TaskPool() {
		waitExecutionCompletion();
		flushWorkers();
	}

	void TaskPool::addTask(Task&& task) {
		std::lock_guard lock(pool_mutex);

		added_tasks.fetch_add(1);

		auto free_worker_opt = getFreeWorkerUnlocked();
		if (free_worker_opt.has_value()) {
			free_worker_opt.value()->scheduleTask([this, pt = std::move(task)](WRef) mutable {
				tryExecuteTask(pt);
			});
			is_worker_free_map[free_worker_opt.value()] = false;
			return;
		} else {
			addToGlobalPoolUnlocked(std::move(task));
		}
	}

	void TaskPool::waitForTask(NodeID id) {
		std::unique_lock lock(pool_mutex);
		task_completed_cv.wait(lock, [this, id] { return isTaskDone(id); });
	}

	void TaskPool::invalidateTask(NodeID id) {
		CORE_ASSERT(
			!id.q_id.getData().isInputQuery(), "Input query nodes are not present in the task pool."
		);

		std::lock_guard lock(pool_mutex);
		auto            val = task_status_map.extract(id);
		CORE_ASSERT(val.has_value(), "Task must be present in the task pool");
		CORE_ASSERT(
			val.value() == TaskStatus::Done, "Invalidating a task that is not done is not supported"
		);
	}

	void TaskPool::waitExecutionCompletion() {
		std::unique_lock lock(pool_mutex);
		task_completed_cv.wait(lock, [this] {
			CORE_ASSERT(
				added_tasks.load() >= completed_tasks.load(),
				"Completed tasks cannot exceed added tasks"
			);
			return added_tasks.load() == completed_tasks.load();
		});
	}

	void TaskPool::query(const Task& task) {
		// @TODO: #2035 we could add fast path here, that checks if the task is already done,
		// as ->query is performing a lot of operations even in such case

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

		// this insert decided who get's to do the task
		if (change_status_result.toOpt().has_value()) {
			// The key was inserted by us, we can execute the task
			auto wd = concurrent::worker::Worker::getCurrentWorker();

			task.work(wd);
			{
				std::lock_guard lock(pool_mutex);

				task_status_map.update(task.id, TaskStatus::Done);
				completed_tasks.fetch_add(1);
			}
			task_completed_cv.notify_all();
			return true;
		}

		// This line, although maybe counter intuitive on the first sight, is correct.
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
		WRef       current_worker = concurrent::worker::Worker::getCurrentWorker();
		const auto task_id        = task.id;

		// This line is not needed, but it sometimes avoids scheduling duplicate tasks
		if (task_status_map.contains(task_id)) return TaskHandle(*this, task_id);

		// Now because we are adding a new task to the pool, we increment the added tasks counter.
		added_tasks.fetch_add(1);

		{
			std::lock_guard lock(pool_mutex);
			auto            free_worker_opt = getFreeWorkerUnlocked();

			// @note It is possible that the free worker would not be free by the time we will
			// schedule a task on it. This would be unfortunate, but not a problem, as the worker
			// manager will just queue the task for later execution.

			if (free_worker_opt.has_value()) {
				// Schedule on a free worker
				free_worker_opt.value()->scheduleTask([this, pt = std::move(task)](WRef) mutable {
					tryExecuteTask(pt);
				});
				is_worker_free_map[free_worker_opt.value()] = false;
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

	void TaskPool::await(NodeID id) {
		// @TODO: #2035 we could add fast path here, that checks if the task is already done,
		// as ->await is performing a lot of operations even in such case

		std::unique_lock lock(pool_mutex);
		auto             task_opt
			= tryStealFromWorkerUnlocked(concurrent::worker::Worker::getCurrentWorker(), id);
		if_opt_some(task_opt, task) {
			lock.unlock();
			tryExecuteTask(task);
			lock.lock();
		}
		task_completed_cv.wait(lock, [this, id] { return isTaskDone(id); });
	}

	bool TaskPool::isTaskDone(NodeID id) const {
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

	base::Optional<Task> TaskPool::tryStealFromWorkerUnlocked(WRef worker_ref) {
		auto& worker_pool = worker_pools[worker_ref];
		if (!worker_pool.empty()) {
			Task task = std::move(worker_pool.front());
			worker_pool.pop_front();
			return task;
		}

		return std::nullopt;
	}

	base::Optional<Task> TaskPool::tryStealFromWorkerUnlocked(WRef worker_ref, NodeID task_id) {
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

	void TaskPool::addToWorkerPoolUnlocked(WRef worker_ref, Task&& task) {
		worker_pools[worker_ref].push_back(std::move(task));
	}

	void TaskPool::onWorkerNoTasks(WRef current_worker) {
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
			current_worker->scheduleTask([this, pt = std::move(task_opt).value()](WRef) mutable {
				tryExecuteTask(pt);
			});
			is_worker_free_map[current_worker] = false;
		} else {
			// There might be some tasks scheduled before this callback get's cpu time,
			// so we set the worker as free if he has no tasks scheduled.
			is_worker_free_map[current_worker] = not current_worker->internalHasTasks();
		}
	}

	void TaskHandle::await() { pool.await(task_id); }

	base::Optional<concurrent::worker::WRef> TaskPool::getFreeWorkerUnlocked() const {
		for (const auto& [worker_ref, is_free]: is_worker_free_map)
			if (is_free.load()) return worker_ref;
		return std::nullopt;
	}

	void TaskPool::flushWorkers() {
		auto                                           workers = worker_manager.getAllWorkers();
		std::vector<std::shared_ptr<std::atomic_bool>> flags;

		for (auto& w: workers) {
			auto flag = std::make_shared<std::atomic_bool>(false);
			flags.push_back(flag);
			worker_manager.setNoTasksCallback(w, [flag](concurrent::worker::WRef) {
				flag->store(true);
			});
		}

		for (auto wref: workers) {
			wref->scheduleTask([](concurrent::worker::WRef) {
				// empty task
			});
		}

		for (auto& flag: flags)
			while (!flag->load()) std::this_thread::yield();
	}
}
