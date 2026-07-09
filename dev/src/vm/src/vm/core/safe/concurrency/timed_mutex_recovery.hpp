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
	inline void clearAbandonedLock([[maybe_unused]] std::timed_mutex& mutex) {
#ifdef __APPLE__
		// After try_lock() the internal locked flag is set whether the mutex was free or already
		// held, so the following unlock() always satisfies the destructor's invariant.
		(void) mutex.try_lock();
		mutex.unlock();
#endif
	}
}
