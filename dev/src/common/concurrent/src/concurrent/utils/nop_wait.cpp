
#include "nop_wait.hpp"

#include <base/config/target_info.hpp>

#if BASE_TARGET_ARCH_X86
	#include <immintrin.h>
#elif BASE_TARGET_ARCH_ARM
	#if BASE_TARGET_COMPILER_MSVC
		// Microsoft Visual C++ on ARM
		#include <intrin.h>
		#define _mm_pause() __yield()
	#else
		// GCC & Clang on ARM
		#define _mm_pause() __asm__ __volatile__("yield" ::: "memory")
	#endif
#else
	#error "Unsupported architecture"
#endif

namespace concurrent {
	/**
	 * A simple busy wait implementation.
	 * Used by some lock primitives.
	 */
	void nopWait(u64 repeat) noexcept {
		for (u64 i = 0; i < repeat; i++) _mm_pause();
	}
}
