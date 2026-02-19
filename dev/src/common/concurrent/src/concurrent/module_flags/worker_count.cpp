#include "worker_count.hpp"

#include <base/except/exceptions.hpp>

#include <atomic>
#include <iostream>

#include <concurrent/base/locks/atomic_flag_spinlock.hpp>

// XD TODO revert it:
namespace concurrent {

	AtomicFlagSpinlock::Printer AtomicFlagSpinlock::printer{};
	
	constinit std::atomic<u64> AtomicFlagSpinlock::Printer::wait_hit{ 0 };
	constinit std::atomic<u64> AtomicFlagSpinlock::Printer::all_uses{ 0 };

} // namespace concurrent

namespace concurrent::worker {

	namespace {
		constinit std::atomic<u64> worker_count{ 0 };
	}

	void setWorkerCount(u64 value) {
		auto previous = worker_count.exchange(value);
		CORE_ASSERT(previous == 0, "Worker count can only be set once.");
	}

	u64 getWorkerCount() {
		auto count = worker_count.load();
		if (count == 0) {
			setWorkerCount(1);
			count = 1;

			// This intentionally does not use the logger as this is a temporary warning,
			// not a proper log, as well as to keep concurrent module dependent only on base.
			std::cerr << "Warning: Worker count was not set, defaulting to 1 worker.\n";
		}

		CORE_ASSERT(count != 0, "Worker count has not been set.");

		return count;
	}

}
