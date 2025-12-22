#pragma once

#include <base/types/ints.hpp>
#include <immintrin.h>

namespace concurrent {
	/**
	 * A simple busy wait implementation.
	 * Used by some lock primitives.
	 */
	void nopWait(u64 repeat) noexcept;
}
