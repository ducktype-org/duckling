#include <iostream>

#include "../init_guard.hpp"

namespace base::detail {
	void logInitFunction(const char* function_name) {
		// @TODO: Wrap it into some generic logger
		// module, that we will use compiler-wide:
		std::cerr << "Initializing: " << function_name << '\n';
	}
}
