
#include "nop_wait.hpp"
#include <immintrin.h>

namespace concurrent {
	/**
	 * A simple busy wait implementation.
	 * Used by some lock primitives.
	 */
    [[gnu::noinline]]
	void nopWait(u64 repeat) noexcept {
		for (u64 i = 0; i < repeat; i++) {
			// asm volatile(R"(
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            //     nop
            // )");
            _mm_pause();
		}
	}
}
