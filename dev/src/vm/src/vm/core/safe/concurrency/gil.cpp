// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "gil.hpp"

#include <chrono>

namespace vm {
	void GIL::acquire() {
		std::unique_lock lock(state_mutex);
		if (!held && next_ticket == now_serving) {
			held = true;
			return;
		}

		const u64 ticket                 = next_ticket++;
		u64       saved_exchange_counter = exchange_counter.load(std::memory_order_relaxed);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(TIMEOUT_MS);

		while (held || now_serving != ticket) {
			if (state_cv.wait_until(lock, deadline) != std::cv_status::timeout) continue;

			// Ask the holder to release only if it kept the GIL for the whole timeout.
			const u64 current_counter = exchange_counter.load(std::memory_order_relaxed);
			if (current_counter == saved_exchange_counter)
				release_requested_flag.store(true, std::memory_order_relaxed);
			else
				saved_exchange_counter = current_counter;
			deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(TIMEOUT_MS);
		}
		now_serving++;
		held = true;
	}

	void GIL::release() {
		{
			std::lock_guard lock(state_mutex);
			held = false;
			exchange_counter.fetch_add(1, std::memory_order_relaxed);
		}
		state_cv.notify_all();
	}

	bool GIL::shouldRelease() {
		if (release_requested_flag.load(std::memory_order_relaxed)) {
			release_requested_flag.store(false, std::memory_order_relaxed);
			return true;
		}
		return false;
	}
}
