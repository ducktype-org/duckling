#include "worker_count.hpp"

#include <base/except/exceptions.hpp>

namespace concurrent {

	namespace {
		constinit u64 worker_count = 0;
	}

	void setWorkerCount(u64 value) {
		CORE_ASSERT(worker_count == 0, "Worker count can only be set once.");
		worker_count = value;
	}

	u64 getWorkerCount() {
		CORE_ASSERT(worker_count != 0, "Worker count has not been set.");
		return worker_count;
	}

}
