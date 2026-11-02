// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <version>  // IWYU pragma: keep

#ifdef __cpp_lib_stacktrace

	#include <stacktrace>
	#include <string>

namespace base {
	std::string prettyStacktraceString(const std::stacktrace&);
}

#endif
