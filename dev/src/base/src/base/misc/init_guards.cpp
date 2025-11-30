#include "init_guard.hpp"

#include <base/module_flags/module_flags.hpp>

#include <iostream>

namespace base::internal {
	void logInitFunction(const char* function_name) {
		// Note that we dont use logging module here,
		// since base should have no dependencies on other modules.
		if constexpr (ENABLE_DEV_LOGS) std::cerr << "Initializing: " << function_name << '\n';
	}
}
