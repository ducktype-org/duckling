// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include <base/config/target_info.hpp>

#if BASE_TARGET_PLATFORM_POSIX
	#include <cstdlib>
	#include <cstring>
	#include <string>
#endif


namespace os_utils {

#if BASE_TARGET_PLATFORM_POSIX
	int threadSafeSetenv(const char* name, const char* value, int overwrite);

	std::string threadSafeStrsignal(int signum);
#endif
};
