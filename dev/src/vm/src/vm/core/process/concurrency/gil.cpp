#include "gil.hpp"

namespace vm {
	void GIL::acquire() {
		u64 saved_gil_switch_counter = gil_exchanges.load(std::memory_order_relaxed);

		while (true) {
			// We wait for a guaranteed quant of time, and if gil is not released during this time,
			// we set the flag to ask the thread holding gil to release it.
			if (gil.try_lock_for(std::chrono::milliseconds(GIL_TIMEOUT_MS))) return;

			u64 current_counter = gil_exchanges.load(std::memory_order_relaxed);
			if (current_counter == saved_gil_switch_counter)
				gil_timeout_flag.store(true, std::memory_order_relaxed);
			else
				saved_gil_switch_counter = current_counter;
		}
	}

	void GIL::release() {
		gil_exchanges.fetch_add(1, std::memory_order_relaxed);
		gil.unlock();
	}

	bool GIL::shouldRelease() {
		return gil_timeout_flag.exchange(false, std::memory_order_relaxed);
	}
}
