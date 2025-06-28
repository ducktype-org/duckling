#pragma once

#include <stacktrace>
#include <string>

namespace base {
	std::string prettyStacktraceString(const std::stacktrace&);
}
