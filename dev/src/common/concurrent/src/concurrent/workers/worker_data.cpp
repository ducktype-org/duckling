

#include "worker_data.hpp"

#include <concurrent/module_flags/worker_count.hpp>

namespace concurrent {

	WorkerData::WorkerData(u64 id, std::mt19937_64 rng): id(id), rng(rng) {}

	CRef<std::vector<WDRef>> WorkerData::generateWorkerData() {
		static std::vector<Box<WorkerData>> worker_data_instances;
		static std::vector<WDRef>           worker_data_references;

		u64 worker_count = concurrent::getWorkerCount();
		worker_data_instances.reserve(worker_count);
		worker_data_references.reserve(worker_count);

		for (u64 i = 0; i < worker_count; i++) {
			Box<WorkerData> worker_data = makeBox<WorkerData>(i, std::mt19937_64(i * 123'456));

			worker_data_instances.emplace_back(std::move(worker_data));
			worker_data_references.emplace_back(worker_data_instances.back().refMut());
		}

		return &worker_data_references;
	}

	CRef<std::vector<WDRef>> WorkerData::getWorkerData() {
		// wrapping this call in static ensures thread-safe one-time initialization:
		static CRef<std::vector<WDRef>> worker_data = generateWorkerData();

		return worker_data;
	}

}
