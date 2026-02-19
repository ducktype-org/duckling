#include "task_pool.hpp"

#include <concurrent/worker/worker.hpp>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>
#include <timer/timer.hpp>

#include <atomic>
#include <iostream>
#include <mutex>

namespace query::internal {

	TaskPool::TaskPool():
		  worker_manager(concurrent::worker::WorkerManager::get()),
		  num_workers(worker_manager.getAllWorkers().size()),
		task_completed_mutexes(TASK_SHARDS),
		  task_completed_cvs(TASK_SHARDS) {

		// Initialize per-worker pools
		for (auto worker: worker_manager.getAllWorkers())
			worker_pools.emplace(worker, std::deque<Task>());

		for (auto worker: worker_manager.getAllWorkers()) is_worker_free_map.put(worker, true);

		// @TODO: #2039 this links query task execution with workers manager logic.
		worker_manager.setNoTasksCallback([this](auto wref) { onWorkerNoTasks(wref); });
	}

	TaskPool::~TaskPool() {
		flushWorkers();
	}

	void TaskPool::addTask(Task&& task) {
		std::lock_guard lock(pool_mutex);


		auto chosen_worker
			= worker_manager.scheduleTaskOnAnyWorker([this, pt = std::move(task)](WRef) mutable {
				  tryExecuteTask(pt);
			  });

		is_worker_free_map[chosen_worker] = false;
	}

	void TaskPool::waitForTask(NodeID id) {
		struct WaitTimePrinter final {
			timer::AtomicDuration wait_time;

			~WaitTimePrinter() {
				std::cerr << "Total wait time for tasks in TaskPool: ";
				timer::printAs(std::cerr, wait_time.toDuration(), timer::TimeUnit::Milliseconds);
				std::cerr << "\n";
			}
		};
		static  WaitTimePrinter wait_time_printer;

		timer::TimeMeasurement tm;
		tm.startMeasurement();

		auto& mutex = task_completed_mutexes[taskHash(id) % TASK_SHARDS];
		auto& cv = task_completed_cvs[taskHash(id) % TASK_SHARDS];
		std::unique_lock lock(mutex);
		cv.wait(lock, [this, id] { return isTaskDone(id); });

		tm.endMeasurement();
		wait_time_printer.wait_time.add(tm.duration());
	}


	void TaskPool::execute() { CORE_UNREACHABLE(); }

	void TaskPool::query(const Task& task) {
		// @TODO: #2035 we could add fast path here, that checks if the task is already done,
		// as ->query is performing a lot of operations even in such case

		
		if (auto task_status = task_status_map.getCurrent(task.id)) {
			if (*task_status == TaskStatus::Done) {
				return;
			}
			else if (*task_status == TaskStatus::InProgress) {
				waitForTask(task.id);
				return;
			}
		}

		bool task_done = tryExecuteTask(task);

		if (not task_done) {
			// auto& 		   mutex = task_completed_mutexes[taskHash(task.id) % TASK_SHARDS];
			// auto& cv = task_completed_cvs[taskHash(task.id) % TASK_SHARDS];

			// const auto       id = task.id;
			// std::unique_lock lock(mutex);
			// cv.wait(lock, [this, id] { return isTaskDone(id); });
			waitForTask(task.id);
		}
	}

	bool TaskPool::tryExecuteTask(const Task& task) {
		// @note This can also be achieved via setting to `NotStarted` and then using
		// compareAndExchange(expected=NotStarted, desired=InProgress) and this is strongly
		// preferred by the LLM models (but using `maybePut` has the same semantics but on adding
		// instead of comparing).

		auto change_status_result = task_status_map.addIfNotExists(task.id);

		// this insert decided who get's to do the task
		if (change_status_result) {
			// The key was inserted by us, we can execute the task
			auto wd = concurrent::worker::Worker::getCurrentWorker();

			task.work(wd);
			task_status_map.setDone(task.id);
			
			auto& cv = task_completed_cvs[taskHash(task.id) % TASK_SHARDS];
			cv.notify_all();
			return true;
		}

		// do we need this notify?:
		// cv.notify_all();
		
		return false;
	}

	TaskHandle TaskPool::schedule(Task&& task) {
		// This function is the most problematic in terms of using independent queues,
		WRef       current_worker = concurrent::worker::Worker::getCurrentWorker();
		const auto task_id        = task.id;

		// This line is not needed, but it sometimes avoids scheduling duplicate tasks
		if (task_status_map.contains(task_id)) return TaskHandle(*this, task_id);



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

		if (auto task_status = task_status_map.getCurrent(id)) {
			if (*task_status == TaskStatus::Done) {
				return;
			}
			else if (*task_status == TaskStatus::InProgress) {
				waitForTask(id);
				return;
			}
		}

		auto& mutex = task_completed_mutexes[taskHash(id) % TASK_SHARDS];
		
		mutex.lock();
		auto             task_opt
			= tryStealFromWorkerUnlocked(concurrent::worker::Worker::getCurrentWorker(), id);
		mutex.unlock();
			
		if_opt_some(task_opt, task) {
			tryExecuteTask(task);
		}

		waitForTask(id);
	}

	bool TaskPool::isTaskDone(NodeID id) const {
		// @TODO: #1973 integrate with query.
		return task_status_map.isDone(id);
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
