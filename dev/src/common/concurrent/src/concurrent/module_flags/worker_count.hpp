#pragma once

#include <base/types/ints.hpp>

namespace concurrent::worker {
	/**
	 * Sets the (max) number of workers (i.e. threads) present in the system.
	 * This function can only be called once and must be called before
	 * most other functionalities of the concurrent module are used.
	 */
	void setWorkerCount(u64 value);

	/**
	 * Gets the (max) number of workers present in the system.
	 * This function can only be called after setWorkerCount has been called.
	 */
	u64 getWorkerCount();
}
