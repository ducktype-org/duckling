

#include "worker_data.hpp"
#include <concurrent/types/atomic_u64.hpp>
#include <concurrent/module_flags/worker_count.hpp>

namespace concurrent {

	WorkerData::WorkerData(u64 id, std::mt19937_64 rng)
		: id(id), rng(rng) {}
	
	Box<WorkerData> WorkerData::make() {
		static AtomicU64 next_id{0};

		u64 id = next_id.inc();
		std::mt19937_64 rng_engine(id * 123456);

		CORE_ASSERT(id < concurrent::getWorkerCount(), "Worker id exceeds worker count");

		return makeBox<WorkerData>(id, rng_engine);
	}

}
