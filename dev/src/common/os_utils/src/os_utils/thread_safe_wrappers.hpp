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
#endif

#include <mutex>
#include <string>

using std::literals::operator""s;

namespace os_utils {

#if BASE_TARGET_PLATFORM_POSIX
	inline int threadSafeSetenv(const char* name, const char* value, int overwrite) {
		static std::mutex      mutex{};
		const std::scoped_lock guard(mutex);
		// NOLINTNEXTLINE(concurrency-mt-unsafe),
		return setenv(name, value, overwrite);
	}

	inline std::string threadSafeStrsignal(int signum) {
		static std::mutex      mutex{};
		const std::scoped_lock guard(mutex);
		// NOLINTNEXTLINE(concurrency-mt-unsafe),
		const char* result = strsignal(signum);
		if (result == nullptr) return "invalid signal number"s;
		return result;
	}
#endif
};
