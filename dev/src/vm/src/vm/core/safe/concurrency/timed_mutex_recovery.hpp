#pragma once

#include <mutex>

namespace vm {
	/**
	 * @brief Clears an abandoned lock on a std::timed_mutex before it is destroyed.
	 *
	 * macOS lacks pthread_mutex_timedlock, so libstdc++'s std::timed_mutex falls back to a
	 * mutex + condition_variable + bool implementation whose destructor asserts the mutex is not
	 * locked (these assertions are enabled by default for -O0 builds). A DVM thread that panics
	 * or is killed while holding a lock leaves it set, so the pooled mutex would abort at
	 * teardown. The native Linux timed_mutex has no such destructor assertion, hence this is a
	 * no-op there. Teardown is single-threaded (all execution threads are joined before the
	 * pools are destroyed), so clearing an abandoned lock here is safe.
	 */
	inline void clearAbandonedLock([[maybe_unused]] std::timed_mutex& mutex) noexcept {
#ifdef __APPLE__
		// PRECONDITION: teardown is single-threaded (all DVM execution threads are already
		// joined), so nothing else contends this mutex. try_lock() leaves the fallback's internal
		// locked flag set whether the mutex was free or abandoned-locked, so the following
		// unlock() clears it and satisfies the destructor's `!_M_locked` invariant. This relies
		// on the libstdc++ fallback timed_mutex, which does not track an owner, hence macOS-only.
		// Best-effort: run under try/catch so a stray failure never escapes the destructor this
		// is called from; the worst case is the original assertion firing, no worse than a no-op.
		try {
			(void) mutex.try_lock();
			mutex.unlock();
		} catch (...) {
			// Nothing safe to do at teardown; leave the mutex as-is.
		}
#endif
	}
}
