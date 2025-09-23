#pragma once

#include <base/ints.hpp>

namespace concurrent {
	/**
	 * Maximum number of workers supported.
	 * Each worker can only operate as if it has its own thread.
	 */
	constexpr u64 MAX_WORKERS = 128;
}
