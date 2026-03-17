
#include "nop_wait.hpp"

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
	#include <immintrin.h>
#elif defined(__aarch64__) || defined(__arm__) || defined(_M_ARM64) || defined(_M_ARM)
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
