#pragma once




#ifdef BUILD_TYPE_DEV_DEBUG
namespace config {
	inline bool debug_mode = false;
}

	#define DEBUG_LOG(x)                                        \
		do {                                                    \
			if (config::debug_mode) { std::cerr << x << "\n"; } \
		} while (0)

#else
	#define DEBUG_LOG(x)
#endif
