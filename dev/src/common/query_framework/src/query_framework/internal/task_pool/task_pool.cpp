#include "task_pool.hpp"

#include <concurrent/worker/worker.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/ints.hpp>

#include <algorithm>
#include <atomic>
#include <mutex>

namespace query::internal {

	namespace {
		usize taskHash(const query::internal::NodeIDID& node) {
			return std::hash<query::internal::NodeIDID>{}(node);
		}
	}

	TaskPool::TaskPool():
		  worker_manager(concurrent::worker::WorkerManager::get()),
		  num_workers(worker_manager.getAllWorkers().size()),
		  worker_pools(num_workers),
		  is_worker_free(num_workers) {
		std::ranges::fill(is_worker_free, true);

		u64 worker_index = 0;
		for (auto worker: worker_manager.getAllWorkers()) {
			CORE_ASSERT(
				worker_index++ == worker->getID(), "Worker IDs must be sequential starting from 0"
			);
		}
	}

	TaskPool::~TaskPool() {
		// We need to make sure, that no worker is executing a task from this pool before we destroy
		// it, otherwise we might have a use-after-free. We can ensure this by waiting for all
		// workers to be free, which means that they are not executing any task from this pool. This
		// will also wait for finishing the taks not sheduled by TaskPool but there is no other way
		// to do that
		concurrent::worker::WorkerManager::get().waitForAllWorkersFree(std::chrono::milliseconds(10)
		);
	}

	void TaskPool::addTask(Task&& task) {
		auto free_worker_opt = getFreeWorker();
		if (free_worker_opt.has_value()) {
			free_worker_opt.value()->scheduleTask([this, task = std::move(task)](WRef) mutable {
				tryExecuteTask(task);
				onWorkerNoTasks();
			});
			return;
		}
		addToGlobalPool(std::move(task));
		free_worker_opt = getFreeWorker();
		if (free_worker_opt.has_value()) {
			free_worker_opt.value()->scheduleTask([this](WRef) {
				if (auto task_opt = tryStealFromGlobal()) tryExecuteTask(task_opt.value());
				onWorkerNoTasks();
			});
		}
	}

	void TaskPool::waitForTask(NodeIDID id) {
		auto             task_hash  = taskHash(id);
		auto&            task_mutex = task_completed_mutexes.at(task_hash % TASK_SHARDS);
		auto&            task_cv    = task_completed_cv.at(task_hash % TASK_SHARDS);
		std::unique_lock lock(task_mutex);
		task_cv.wait(lock, [this, id] { return isTaskDone(id); });
	}

	void TaskPool::invalidateTask(NodeIDID id) {
		CORE_ASSERT(
			!id.getID().q_id.getData().isInputQuery(), "Input query nodes are not present in the task pool."
		);

		auto val = task_status_map.getRef(id)->status.exchange(TaskStatus::NotStarted);
		// CORE_ASSERT(val.has_value(), "Task must be present in the task pool");
		CORE_ASSERT(
			val == TaskStatus::Done, "Invalidating a task that is not done is not supported"
		);
	}

	void TaskPool::query(const Task& task) {
		bool task_done = tryExecuteTask(task);
		if (not task_done) waitForTask(task.id);
	}

	bool TaskPool::tryExecuteTask(const Task& task) {
		// @note This can also be achieved via setting to `NotStarted` and then using
		// compareAndExchange(expected=NotStarted, desired=InProgress) and this is strongly
		// preferred by the LLM models (but using `maybePut` has the same semantics but on adding
		// instead of comparing).

		// bool task_already_done = false;

		auto task_status_ref = task_status_map.getRef(task.id);

		TaskStatus what_status_was_there = TaskStatus::NotStarted;
		bool did_we_put_in_progress = task_status_ref->status.compare_exchange_strong(what_status_was_there, TaskStatus::InProgress);
		
		// task_status_map.maybePutAndUpdate(
		// 	task.id,
		// 	TaskStatus::InProgress,
		// 	[&task_already_done](base::CRef<TaskStatus> existing_status) {
		// 		if (*existing_status == TaskStatus::Done) {
		// 			task_already_done = true;
		// 			return;
		// 		}
		// 	}
		// );

		if (what_status_was_there == TaskStatus::Done) {
			// The task was already done, we can return immediately
			return true;
		}

		// This insert decides who gets to execute the task.
		if (did_we_put_in_progress) {
			// The key was inserted by us, we can execute the task
			auto wd = concurrent::worker::Worker::getCurrentWorker();

			task.work(wd);

			task_status_ref->status.store(TaskStatus::Done, std::memory_order_release);
			{
				auto  task_hash  = taskHash(task.id);
				auto& task_mutex = task_completed_mutexes.at(task_hash % TASK_SHARDS);
				auto& task_cv    = task_completed_cv.at(task_hash % TASK_SHARDS);

				std::lock_guard lock(task_mutex);
				task_cv.notify_all();
			}
			return true;
		}

		return false;
	}

	TaskHandle TaskPool::schedule(Task&& task) {
		// This function is the most problematic in terms of using independent queues,
		WRef       current_worker = concurrent::worker::Worker::getCurrentWorker();
		const auto task_id        = task.id;

		// This line is not needed, but it sometimes avoids scheduling duplicate tasks
		if (task_status_map.getRef(task_id)->status != TaskStatus::NotStarted) return TaskHandle(*this, task_id);

		// Add to current worker's pool
		addToWorkerPool(current_worker, std::move(task));
		auto free_worker_opt = getFreeWorker();
		if (free_worker_opt.has_value()) {
			free_worker_opt.value()->scheduleTask([this, current_worker](WRef) {
				if (auto task_opt = tryStealFromWorker(current_worker))
					tryExecuteTask(task_opt.value());
				onWorkerNoTasks();
			});
		}

		return TaskHandle(*this, task_id);
	}

	void TaskPool::await(NodeIDID id) {
		// Fast path check without locking
		auto pre_status = task_status_map.getRef(id)->status.load(std::memory_order_acquire);
		if (pre_status == TaskStatus::Done) {
			return;
		} else if (pre_status == TaskStatus::InProgress) {
			waitForTask(id);
			return;
		}
	
		auto task_opt = tryStealFromWorker(concurrent::worker::Worker::getCurrentWorker(), id);

		if_opt_some(task_opt, task) {
			if (tryExecuteTask(task)) return;
		}
		waitForTask(id);
	}

	bool TaskPool::isTaskDone(NodeIDID id) {
		auto status = task_status_map.getRef(id)->status.load(std::memory_order_acquire);
		return status == TaskStatus::Done;
	}

	base::Optional<Task> TaskPool::tryStealFromGlobal() { return global_pool.tryPop(); }

	base::Optional<Task> TaskPool::tryStealFromWorker(WRef worker_ref) {
		return worker_pools[worker_ref->getID()].tryPop();
	}

	base::Optional<Task> TaskPool::tryStealFromWorker(WRef worker_ref, NodeIDID task_id) {
		auto& worker_pool = worker_pools[worker_ref->getID()];
		return worker_pool.extractIf([&](base::CRef<Task> task) { return task->id == task_id; });
	}

	void TaskPool::addToGlobalPool(Task&& task) { global_pool.push(std::move(task)); }

	void TaskPool::addToWorkerPool(WRef worker_ref, Task&& task) {
		auto& worker_pool = worker_pools[worker_ref->getID()];
		worker_pool.push(std::move(task));
	}

	void TaskPool::onWorkerNoTasks() {
		auto current_worker = concurrent::worker::Worker::getCurrentWorker();
		// Set the worker as free
		is_worker_free[current_worker->getID()].store(true, std::memory_order_seq_cst);

		auto set_worker_not_free = [&]() -> bool {
			bool expected = true;
			return is_worker_free[current_worker->getID()].compare_exchange_strong(
				expected, false, std::memory_order_seq_cst
			);
		};

		base::Optional<Task> task_opt;
		bool                 was_free = true;

		// First try to steal from the worker's local pool
		task_opt = worker_pools[current_worker->getID()].tryPopIf([&](base::CRef<Task>) {
			was_free = set_worker_not_free();
			return was_free;
		});

		if (!was_free) {
			// We failed to set ourselves as not free, which means that another thread is scheduling
			// a task on us, so we should not steal any tasks, as we will get a task scheduled on us
			// soon.
			return;
		}

		// If failed, try to steal from the global pool
		if (task_opt.empty()) {
			task_opt = global_pool.tryPopIf([&](base::CRef<Task>) {
				was_free = set_worker_not_free();
				return was_free;
			});

			if (!was_free) return;
		}

		// If failed, try to steal from other workers
		if (task_opt.empty()) {
			for (auto worker_ref: worker_manager.getAllWorkers()) {
				task_opt = worker_pools[worker_ref->getID()].tryPopIf([&](base::CRef<Task>) {
					was_free = set_worker_not_free();
					return was_free;
				});
				if (!was_free) return;
				if (task_opt.has_value()) break;
			}
		}

		if (task_opt.has_value()) {
			current_worker->scheduleTask([this, task = std::move(task_opt).value()](WRef) mutable {
				tryExecuteTask(task);
				onWorkerNoTasks();
			});
		}
	}

	void TaskHandle::await() { pool.await(task_id); }

	base::Optional<concurrent::worker::WRef> TaskPool::getFreeWorker() {
		auto it = std::ranges::find_if(is_worker_free, [](std::atomic<bool>& is_free) {
			bool expected = true;
			return is_free.compare_exchange_strong(expected, false, std::memory_order_seq_cst);
		});
		if (it != is_worker_free.end()) {
			auto index = static_cast<size_t>(std::distance(is_worker_free.begin(), it));
			return worker_manager.getAllWorkers()[index];
		}
		return {};
	}
}
