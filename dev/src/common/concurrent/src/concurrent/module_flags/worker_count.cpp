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
		constinit std::atomic<u64> worker_count{ 0 };
	}

	void setWorkerCount(u64 value) {
		auto previous = worker_count.exchange(value);
		CORE_ASSERT(previous == 0, "Worker count can only be set once.");
	}

	u64 getWorkerCount() {
		auto count = worker_count.load(std::memory_order_relaxed);
		if (count == 0) {
			setWorkerCount(1);
			count = 1;

			// @TODO: #2038 likely change that.
			// This intentionally does not use the logger as this is a temporary warning,
			// not a proper log, as well as to keep concurrent module dependent only on base.
			std::cerr << "Warning: Worker count was not set, defaulting to 1 worker.\n";
		}

		CORE_ASSERT(count != 0, "Worker count has not been set.");

		return count;
	}

}
