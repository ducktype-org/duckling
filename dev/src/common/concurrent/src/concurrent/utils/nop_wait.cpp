
#include "nop_wait.hpp"
// @TODO: #2308 Use the mentioned flags here
#if defined(__x86_64__) || defined(__i386__)
	#include <immintrin.h>
#elif defined(__aarch64__) || defined(__arm__)
	#if defined(_MSC_VER)
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
