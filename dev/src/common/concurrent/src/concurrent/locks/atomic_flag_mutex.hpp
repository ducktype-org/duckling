/**
* This file contains a simple mutex implementation based on atomic_flag.
* This implementation is adapted from the implementation present at
* https://en.cppreference.com/w/cpp/atomic/atomic_flag.html
*/

#pragma once

#include <atomic>
#include <version>

// we test that implementation provides atomic wait/notify functionality:
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201'907L
    // OK
#else
    #error "AtomicFlagMutex requires atomic wait/notify functionality (C++20)."
#endif

namespace concurrent {
    
    /**
     * Simple enum representing the result of a tryLock operation.
     */
    enum class TryLockResult {
        Acquired,
        NotAcquired
    };

    /**
     * This is a simple mutex implementation based on atomic_flag.
     * This implementation is adapted from the implementation present at
     * https://en.cppreference.com/w/cpp/atomic/atomic_flag.html
     */
	class AtomicFlagMutex final {
		std::atomic_flag atomic_flag{};
	public:

        /**
         * Acquires the lock, blocking or waiting if necessary.
         */
		void lock() noexcept {
			while (atomic_flag.test_and_set(std::memory_order_acquire))
				// Since C++20, locks can be acquired only after notification in the unlock,
				// avoiding any unnecessary spinning.
				// Note that even though wait guarantees it returns only after the value has
				// changed, the lock is acquired after the next condition check.
				atomic_flag.wait(true, std::memory_order_relaxed);
		}

        /**
         * Tries to acquire the lock without blocking.
         * @return true if the lock was acquired, false otherwise.
         */
		TryLockResult tryLock() noexcept {
            auto res = !atomic_flag.test_and_set(std::memory_order_acquire);
            return res ? TryLockResult::Acquired : TryLockResult::NotAcquired;
        }

        /**
         * Releases the lock.
         */
		void unlock() noexcept {
			atomic_flag.clear(std::memory_order_release);
			atomic_flag.notify_one();
		}
	};
}
