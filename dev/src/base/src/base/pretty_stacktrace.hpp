#pragma once

#include <version>

#ifdef __cpp_lib_stacktrace

	#include <stacktrace>
	#include <string>

namespace base {
	std::string prettyStacktraceString(const std::stacktrace&);
}

#endif
