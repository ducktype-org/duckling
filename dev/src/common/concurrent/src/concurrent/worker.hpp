#include <base/ints.hpp>

namespace concurrent {
	// using WorkerID = u64;

	/**
	 * Each worker -- i.e. thread -- has its own data.
	 * Some operations might require access the data of the worker.
	 */
	struct WorkerData final {
		u64 id; // worker ID

		// rng -- per worker
	};
}
