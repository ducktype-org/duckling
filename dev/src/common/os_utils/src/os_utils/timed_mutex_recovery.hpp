// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/config/target_info.hpp>

#include <mutex>

namespace os_utils {
	/**
	 * @brief Clears an abandoned lock on a std::timed_mutex before it is destroyed.
	 *
	 * @note On Linux this is a no-op. The native pthread implementation of std::timed_mutex
	 * does not assert on abandoned locks at teardown, so no cleanup is needed.
	 *
	 * @note On macOS, libstdc++'s std::timed_mutex falls back to a mutex + condition_variable
	 * + bool implementation whose destructor asserts the mutex is not locked. A DVM thread
	 * that panics or is killed while holding a lock leaves it set, so the pooled mutex would
	 * abort at teardown. This function clears the lock to prevent that abort.
	 *
	 * Teardown is single-threaded (all execution threads are joined before the pools are
	 * destroyed), so clearing an abandoned lock here is safe.
	 */
	inline void clearAbandonedLock(std::timed_mutex& mutex) noexcept {
		if constexpr (not base::IS_TARGET_OS_MACOS) return;

		// Teardown is single-threaded, so nothing contends the mutex here. try_lock()
		// leaves the fallback's internal locked flag set whether the mutex was free or
		// abandoned-locked, so the following unlock() clears it and satisfies the
		// destructor's !_M_locked invariant. Best-effort: never let an exception escape.
		try {
			std::ignore = mutex.try_lock();
			mutex.unlock();
		} catch (...) {
			// Nothing safe to do at teardown; leave the mutex as-is.
		}
	}
}
