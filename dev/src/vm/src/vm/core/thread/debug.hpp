#pragma once

#if !defined(NDEBUG)
namespace config {
	inline bool debug_mode = false;
}
#endif

#if !defined(NDEBUG)
	#define DEBUG_LOG(x)                                        \
		do {                                                    \
			if (config::debug_mode) { std::cerr << x << "\n"; } \
		} while (0)
#else
	#define DEBUG_LOG(x)
#endif
