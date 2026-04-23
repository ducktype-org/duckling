#include "gil.hpp"

#include <atomic>

namespace vm {
	void GIL::acquire() {
		u64 saved_exchange_counter = exchange_counter;

		while (true) {
			// We wait for a guaranteed quant of time, and if GIL is not released during this time,
			// we set the flag to ask the running thread to release GIL.
			if (gil.try_lock_for(std::chrono::milliseconds(TIMEOUT_MS))) return;

			u64 current_counter = exchange_counter;
			if (current_counter == saved_exchange_counter) {
				// If counter is the same as before, it means that GIL was not released during
				// timeout, so we set the flag to ask the running thread to release GIL.
				release_requested_flag = true;
			} else {
				// If counter is different, it means that GIL was released during timeout.
				// In this case we wait again, because we have to give currently running thread
				// guaranteed amount of time before we try to acquire GIL again.
				saved_exchange_counter = current_counter;
			}
		}
	}

	void GIL::release() {
		exchange_counter.fetch_add(1);
		gil.unlock();
	}

	bool GIL::shouldRelease() {
		if (release_requested_flag == true) {
			// Weak meaning may fail spuriously which means,
			// that we might not exchange the flag. This is okay, since
			// this function is called frequently.
			bool expected = true;
			return release_requested_flag.compare_exchange_weak(expected, false);
		}
		return false;
	}
}
