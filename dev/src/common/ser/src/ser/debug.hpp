#pragma once

#include <ser/config.hpp>

#include <cstddef>
#include <cstring>

#if SER_DEBUG_POISON >= 2 && SER_HAS_ASAN
	#include <sanitizer/asan_interface.h>
#endif

// @TODO: #90003 poison released pool slots once the Box/Ref pool exists, nothing calls these yet
namespace ser::internal {

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

}  // namespace ser::internal
