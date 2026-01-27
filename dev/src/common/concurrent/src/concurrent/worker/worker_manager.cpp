#include "worker_manager.hpp"

#include <concurrent/worker/worker.hpp>

#include <ranges>

namespace concurrent::worker {

	WorkerManager::WorkerManager():
		  num_workers(getWorkerCount()),
		  workers({ std::ranges::views::iota(usize{ 0 }, num_workers)
	                | std::ranges::views::transform([](usize i) {
						  auto worker = Box<Worker>::fromPointer(
							  new Worker(WorkerData::getWorkerData()->at(i))
						  );
						  worker->run();
						  return worker;
					  })
	                | std::ranges::to<std::vector<Box<Worker>>>() }) {}

	std::vector<WorkerID> WorkerManager::getAllWorkers() {
		return workers | std::ranges::views::transform([](const Box<Worker>& worker) {
				   return worker->getId();
			   })
		     | std::ranges::to<std::vector<WorkerID>>();
	}

	std::vector<WorkerID> WorkerManager::getFreeWorkers(usize max_count) {
		return workers | std::ranges::views::filter([](const Box<Worker>& worker) {
				   return worker->isFree();
			   })
		     | std::ranges::views::take(max_count)
		     | std::ranges::views::transform([](const Box<Worker>& worker) {
				   return worker->getId();
			   })
		     | std::ranges::to<std::vector<WorkerID>>();
	}

	void WorkerManager::scheduleTaskOnWorker(WorkerID worker_id, Task task) {
		workers[static_cast<usize>(worker_id)]->pushTask(std::move(task));
	}

	void WorkerManager::scheduleTaskOnAnyFreeWorker(Task&& task) {
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

	[[nodiscard]] bool WorkerManager::isWorkerFree(WorkerID worker_id) const {
		return workers[static_cast<usize>(worker_id)]->isFree();
	}

	void WorkerManager::setNoTasksCallback(WorkerID worker_id, NoTasksCallback callback) {
		workers[static_cast<usize>(worker_id)]->setNoTasksCallback(std::move(callback));
	}
}
