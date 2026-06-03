#include "gil.hpp"

namespace vm {
	void GIL::acquire() {
		// First, we try to acquire GIL without waiting. If we succeed, we can return immediately.
		if (gil.try_lock()) return;

		u64 saved_exchange_counter = exchange_counter.load(std::memory_order_relaxed);

		while (true) {
			// We wait for a guaranteed quant of time, and if GIL is not released during this time,
			// we set the flag to ask the running thread to release GIL.
			if (gil.try_lock_for(std::chrono::milliseconds(TIMEOUT_MS))) return;

			u64 current_counter = exchange_counter.load(std::memory_order_relaxed);
			if (current_counter == saved_exchange_counter) {
				// If counter is the same as before, it means that GIL was not released during
				// timeout, so we set the flag to ask the running thread to release GIL.
				release_requested_flag.store(true, std::memory_order_relaxed);
			} else {
				// If counter is different, it means that GIL was released during timeout.
				// In this case we wait again, because we have to give currently running thread
				// guarranteed amount of time before we try to acquire GIL again.
				saved_exchange_counter = current_counter;
			}
		}
	}

	void GIL::release() {
		exchange_counter.fetch_add(1, std::memory_order_relaxed);
		gil.unlock();
	}

	bool GIL::shouldRelease() {
		if (release_requested_flag.load(std::memory_order_relaxed)) {
			release_requested_flag.store(false, std::memory_order_relaxed);
			return true;
		}
		return false;
	}
}
