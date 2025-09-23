#include <base/ints.hpp>

namespace concurrent {

	void nopWait(u64 repeat = 1) {
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
                nop
            )");
		}
	}

}
