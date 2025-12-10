#pragma once

#include <base/types/ints.hpp>

namespace concurrent {
	inline void nopWait(u64 repeat = 1) {
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