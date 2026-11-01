// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "worker_count.hpp"

#include <base/except/exceptions.hpp>

#include <atomic>
#include <iostream>

namespace concurrent::worker {


	namespace {
		static constexpr u64       DEFAULT_WORKER_COUNT = 1;
		constinit std::atomic<u64> worker_count{ 0 };
	}

	void setWorkerCount(u64 value) {
		CORE_ASSERT(value != 0, "Worker count can't be set to zero.");
		auto previous = worker_count.exchange(value);
		CORE_ASSERT(previous == 0, "Worker count can only be set once.");
	}

	u64 getWorkerCount() {
		if (u64 count = worker_count.load(std::memory_order_relaxed); count != 0) return count;
		u64 count = 0;
		// https://en.cppreference.com/c/atomic/atomic_compare_exchange; See `Notes` for a “manual”
		// implementation.
		if (worker_count.compare_exchange_strong(
				count, DEFAULT_WORKER_COUNT, std::memory_order_acq_rel, std::memory_order_acquire
			)) {
			count = DEFAULT_WORKER_COUNT;

			// @TODO: #2038 likely change that.
			// This intentionally does not use the logger as this is a temporary warning,
			// not a proper log, as well as to keep concurrent module dependent only on base.
			std::cerr << "Warning: Worker count was not set, defaulting to 1 worker.\n";
		}

		CORE_ASSERT(count != 0, "Worker count has not been set.");

		return count;
	}

}
