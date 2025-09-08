#pragma once

namespace config {
	inline bool debug_mode = false;
}

#define DEBUG_LOG(x)                                        \
	do {                                                    \
		if (config::debug_mode) { std::cerr << x << "\n"; } \
	} while (0)
