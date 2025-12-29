
#include "nop_wait.hpp"

#include <immintrin.h> // for _mm_pause

namespace concurrent {
	/**
	 * A simple busy wait implementation.
	 * Used by some lock primitives.
	 */
	void nopWait(u64 repeat) noexcept {
		for (u64 i = 0; i < repeat; i++) _mm_pause();
	}
}
