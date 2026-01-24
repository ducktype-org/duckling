#include "worker_manager.hpp"

#include <concurrent/worker/worker.hpp>

#include <ranges>

concurrent::WorkerManager::WorkerManager(concurrent::NoTasksCallback no_tasks_callback):
	  num_workers(getWorkerCount()),
	  workers({ std::ranges::views::iota(usize{ 0 }, num_workers)
                | std::ranges::views::transform([&no_tasks_callback](usize i) {
					  return makeBox<Worker>(WorkerData::getWorkerData()->at(i), no_tasks_callback);
				  })
                | std::ranges::to<std::vector<Box<Worker>>>() }) {
	// We are explicitly starting the workers here to avoid race conditions.
	// Workers could e.g. call the no_tasks_callback before the WorkerManager is fully constructed.
	for (auto& worker: workers) worker->run();
}

std::vector<concurrent::WorkerID> concurrent::WorkerManager::getAllWorkers() {
	return workers
	     | std::ranges::views::transform([](const Box<Worker>& worker) { return worker->getId(); })
	     | std::ranges::to<std::vector<WorkerID>>();
}

std::vector<concurrent::WorkerID> concurrent::WorkerManager::getFreeWorkers(usize max_count) {
	return workers
	     | std::ranges::views::filter([](const Box<Worker>& worker) { return worker->isFree(); })
	     | std::ranges::views::take(max_count)
	     | std::ranges::views::transform([](const Box<Worker>& worker) { return worker->getId(); })
	     | std::ranges::to<std::vector<WorkerID>>();
}

void concurrent::WorkerManager::scheduleTaskOnWorker(WorkerID worker_id, Task task) {
	workers[static_cast<usize>(worker_id)]->pushTask(std::move(task));
}

void concurrent::WorkerManager::scheduleTaskOnAnyFreeWorker(Task&& task) {
	for (auto& worker: workers) {
		if (worker->isFree()) {
			worker->pushTask(std::move(task));
			return;
		}
	}
	// NOLINTBEGIN(concurrency-mt-unsafe)
	// If no free worker is found, push to a random worker
	workers[static_cast<usize>(std::rand()) % num_workers]->pushTask(std::move(task));
	// NOLINTEND(concurrency-mt-unsafe)
}

[[nodiscard]] bool concurrent::WorkerManager::isWorkerFree(WorkerID worker_id) const {
	return workers[static_cast<usize>(worker_id)]->isFree();
}
