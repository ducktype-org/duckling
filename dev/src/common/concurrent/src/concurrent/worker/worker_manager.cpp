#include "worker_manager.hpp"

#include <concurrent/worker/worker.hpp>

#include <mutex>
#include <ranges>

namespace concurrent::worker {

	namespace {
		std::mt19937_64                   rng;
		std::mutex                        mut;

		/**
		 * Helper flag used to ensure that WorkerManager::setWorkers is called only once and before any call to WorkerManager::get().
		 */
		constinit std::atomic_flag is_worker_count_set;
	}

	std::vector<WRef> WorkerManager::getAllWorkers() const {
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

	void WorkerManager::scheduleTaskOnAnyWorker(const Task& task) {
		for (auto& worker: workers)
			if (worker->scheduleTaskIfFree(task)) return;

		// If no free worker is found, push to a random worker
		std::scoped_lock lock(mut);
		workers[static_cast<usize>(rng()) % (workers.size())]->scheduleTask(task);
	}

	bool WorkerManager::isWorkerFree(WRef worker) const { return worker->isFree(); }

	void WorkerManager::setNoTasksCallback(WRef worker, const NoTasksCallback& callback) {
		worker->setNoTasksCallback(callback);
	}

	WorkerManager& WorkerManager::get() {
		static WorkerManager instance;
		CORE_ASSERT(
			is_worker_count_set.test(),
			"WorkerManager::get() called before setting worker count with setWorkers()!"
		);
		return instance;
	}

	void WorkerManager::setWorkers(usize num_workers) {
		auto ware_worker_count_set = is_worker_count_set.test_and_set();
		CORE_ASSERT(
			not ware_worker_count_set,
			"WorkerManager::setWorkers can only be called once and before any call to get()!"
		);

		auto& worker_manager = get();
		worker_manager.workers.clear();
		worker_manager.workers.reserve(num_workers);
		for (usize i = 0; i < num_workers; i++) {
			auto worker = Box<Worker>::fromPointer(new Worker(i));
			worker->run();
			worker_manager.workers.push_back(std::move(worker));
		}
		// Set the seed for the random number generator to ensure different random sequences across runs.
		rng.seed(num_workers);
	}

	void WorkerManager::testPrivateAccessReloadState() {
		auto& worker_manager = get();
		auto  size           = worker_manager.workers.size();
		worker_manager.workers.clear();
		worker_manager.setWorkers(size);
	}
}
