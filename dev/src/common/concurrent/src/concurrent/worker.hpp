#pragma once

#include <base/types/ints.hpp>

#include <random>

namespace concurrent {
	// using WorkerID = u64;

	/**
	 * Each worker -- i.e. thread -- has its own data.
	 * Some operations might require access the data of the worker.
	 */
	struct WorkerData final {
		WorkerData(u64 id, std::minstd_rand rng);

	public:
		WorkerData()                             = delete;
		WorkerData(const WorkerData&)            = delete;
		WorkerData& operator=(const WorkerData&) = delete;
		WorkerData(WorkerData&&)                 = delete;
		WorkerData& operator=(WorkerData&&)      = delete;

		static WorkerData make();

		u64              id;
		std::minstd_rand rng;
	};
}
