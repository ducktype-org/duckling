#pragma once

#include <base/types/ints.hpp>
#include <base/pointers/ref.hpp>
#include <base/pointers/box.hpp>
#include <random>

namespace concurrent {

	/**
	 * Each worker -- i.e. thread -- has its own global worker-local data.
	 * This data can then be used in various places allowing for more efficient
	 * implementations.
	 */
	struct WorkerData final {
		WorkerData(u64 id, std::mt19937_64 rng);

	public:
		WorkerData()                              = delete;
		WorkerData(const WorkerData&)             = delete;
		WorkerData(WorkerData&&)                  = default;

		WorkerData& operator=(const WorkerData&)  = delete;
		WorkerData&       operator=(WorkerData&&) = delete;
		static Box<WorkerData> make();

		u64 id;
		std::mt19937_64 rng;

	};

	using WDRef = Ref<WorkerData>;

}
