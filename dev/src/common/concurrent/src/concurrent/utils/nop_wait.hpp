#pragma once

#include <base/types/ints.hpp>

namespace concurrent {
    /**
     * A simple busy wait implementation.
     * Used by some lock primitives.
     */
	inline void nopWait(u64 repeat) {
		for (u64 i = 0; i < repeat; i++) {
			asm volatile(R"(
                nop
                nop
                nop
                nop
                nop
                nop
                nop
                nop
                nop
                nop
                nop
                nop
                nop
                nop
                nop
                nop
            )");
		}
	}
}
