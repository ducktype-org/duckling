#pragma once

#include <base/types/ints.hpp>

namespace concurrent {
    
    /**
	 * Each worker -- i.e. thread -- has its own global worker-local data.
     * This data can then be used in various places allowing for more efficient
     * implementations.
	 */
	struct WorkerData final {
		WorkerData(u64 id);
	public:
		WorkerData()                             = delete;
		WorkerData(const WorkerData&)            = delete;
		WorkerData& operator=(const WorkerData&) = delete;
		WorkerData(WorkerData&&)                 = delete;
		WorkerData& operator=(WorkerData&&)      = delete;
		static WorkerData make();

        u64              id;
		// std::minstd_rand rng;
	};

}

