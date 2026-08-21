#pragma once

#include <ser/config.hpp>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if SER_DEBUG_POISON >= 2 && SER_HAS_ASAN
	#include <sanitizer/asan_interface.h>
#endif

namespace ser::detail {

	[[noreturn]] inline void assertFailed(
		const char* expr, const char* msg, const char* file, int line
	) noexcept {
		// The assert handler runs when the process is already going down; it must not
		// pull <print> and its locale machinery into every translation unit.
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,modernize-use-std-print)
		::std::fprintf(stderr, "ser: ASSERTION %s\n  %s\n  %s:%d\n", expr, msg, file, line);
		::std::abort();
	}

	inline void poison(void* p, ::std::size_t n) noexcept {
#if SER_DEBUG_POISON >= 1
		::std::memset(p, 0xCD, n);
	#if SER_DEBUG_POISON >= 2 && SER_HAS_ASAN
		__asan_poison_memory_region(p, n);
	#endif
#else
		(void) p;
		(void) n;
#endif
	}

	inline void unpoison([[maybe_unused]] void* p, [[maybe_unused]] ::std::size_t n) noexcept {
#if SER_DEBUG_POISON >= 2 && SER_HAS_ASAN
		__asan_unpoison_memory_region(p, n);
#endif
	}

}  // namespace ser::detail

#ifdef NDEBUG
	#define SER_ASSERT(cond, msg) ((void) 0)
#else
	#define SER_ASSERT(cond, msg) \
		((cond) ? (void) 0 : ::ser::detail::assertFailed(#cond, msg, __FILE__, __LINE__))
#endif
