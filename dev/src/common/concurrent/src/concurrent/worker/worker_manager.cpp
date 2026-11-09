// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "worker_manager.hpp"

#include <concurrent/worker/worker.hpp>

#include <base/except/exceptions.hpp>

#include <mutex>
#include <ranges>

namespace concurrent::worker {

	namespace {
		std::mt19937_64 rng;
		std::mutex      mut;
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

	void WorkerManager::waitForAllWorkersFree(std::chrono::milliseconds sleep_duration) const {
		CORE_ASSERT(
			!Worker::isCurrentThreadWorker(),
			"Cannot call waitForAllWorkersFree from a worker thread"
		);
		while (true) {
			// Recursively locks each worker's mutex in order and checks if all are free.
			// Holding all locks simultaneously ensures a consistent snapshot of worker states.
			auto lock_and_check_all = [&](auto&& self, usize index) -> bool {
				if (index >= workers.size()) return true;

				auto&            worker = workers.at(index);
				std::scoped_lock lock(worker->mut);
				if (!worker->is_free.load(std::memory_order_seq_cst)) return false;

				return self(self, index + 1);
			};

			if (lock_and_check_all(lock_and_check_all, 0)) return;

			std::this_thread::sleep_for(sleep_duration);
		}
	}

	WRef WorkerManager::scheduleTaskOnAnyWorker(Task&& task) {
		for (auto& worker: workers)
			if (worker->scheduleTaskIfFree(std::move(task))) return worker.get();

		// If no free worker is found, push to a random worker
		std::scoped_lock lock(mut);
		auto             id = static_cast<usize>(rng()) % (workers.size());

		workers[id]->scheduleTask(std::move(task));
		return workers[id].get();
	}

	bool WorkerManager::isWorkerFree(WRef worker) const { return worker->isFree(); }

	void WorkerManager::setNoTasksCallback(WRef worker, NoTasksCallback&& callback) {
		worker->setNoTasksCallback(std::move(callback));
	}

	void WorkerManager::setNoTasksCallback(NoTasksCallback&& callback) {
		auto callback_ptr = std::make_shared<NoTasksCallback>(std::move(callback));
		for (auto& worker: getAllWorkers()) worker->setNoTasksCallback(callback_ptr);
	}

	WorkerManager& WorkerManager::get() {
		static WorkerManager instance;
		return instance;
	}

	void WorkerManager::setup(usize num_workers) {
		workers.clear();
		workers.reserve(num_workers);
		for (usize i = 0; i < num_workers; i++) {
			auto worker = Box<Worker>::fromPointer(new Worker(i));
			worker->run();
			workers.push_back(std::move(worker));
		}
		// Set the seed for the random number generator to ensure different random sequences across runs.
		rng.seed(num_workers);
	}

	WorkerManager::WorkerManager() {
		auto num_workers = getWorkerCount();
		setup(static_cast<usize>(num_workers));
	}

	void WorkerManager::testPrivateAccessReloadState() {
		auto& worker_manager = get();
		auto  size           = worker_manager.workers.size();
		worker_manager.setup(size);
	}
}
