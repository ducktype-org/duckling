#include "worker_manager.hpp"

#include <concurrent/worker/worker.hpp>

#include <ranges>

namespace concurrent::worker {

	[[nodiscard]] std::vector<WRef> WorkerManager::getAllWorkers() const {
		return workers
		     | std::ranges::views::transform([](const Box<Worker>& worker) { return worker.get(); })
		     | std::ranges::to<std::vector<WRef>>();
	}

	std::vector<WRef> WorkerManager::getFreeWorkers(usize max_count) const {
		return workers | std::ranges::views::filter([](const Box<Worker>& worker) {
				   return worker->isFree();
			   })
		     | std::ranges::views::take(max_count)
		     | std::ranges::views::transform([](const Box<Worker>& worker) { return worker.get(); })
		     | std::ranges::to<std::vector<WRef>>();
	}

	void WorkerManager::scheduleTaskOnWorker(WRef worker, Task task) {
		worker->scheduleTask(std::move(task));
	}

	void WorkerManager::scheduleTaskOnAnyWorker(Task task) {
		for (auto& worker: workers) {
			if (worker->isFree()) {
				worker->scheduleTask(std::move(task));
				return;
			}
		}
		// NOLINTBEGIN(concurrency-mt-unsafe)
		// If no free worker is found, push to a random worker
		workers[static_cast<usize>(std::rand()) % (workers.size())]->scheduleTask(std::move(task));
		// NOLINTEND(concurrency-mt-unsafe)
	}

	[[nodiscard]] bool WorkerManager::isWorkerFree(WRef worker) const { return worker->isFree(); }

	void WorkerManager::setNoTasksCallback(WRef worker, NoTasksCallback callback) {
		worker->setNoTasksCallback(std::move(callback));
	}

	WorkerManager& WorkerManager::get() {
		static WorkerManager instance;
		return instance;
	}

	void WorkerManager::setWorkers(usize num_workers) {
		auto&& worker_manager = get();
		worker_manager.workers.clear();
		worker_manager.workers.reserve(num_workers);
		for (usize i = 0; i < num_workers; i++) {
			auto worker = Box<Worker>::fromPointer(new Worker(i));
			worker->run();
			worker_manager.workers.push_back(std::move(worker));
		}
	}
}
