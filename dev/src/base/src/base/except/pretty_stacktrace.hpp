#pragma once

#include <version>  // IWYU pragma: keep

#ifdef __cpp_lib_stacktrace

	#include <stacktrace>
	#include <string>

namespace base {
	std::string prettyStacktraceString(const std::stacktrace&);
}

#endif
