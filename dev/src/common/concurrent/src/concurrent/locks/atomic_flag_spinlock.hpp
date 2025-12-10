/**
* This file contains a simple spin lock implementation based on atomic_flag.
* This implementation is adapted from the implementation present at
* https://en.cppreference.com/w/cpp/atomic/atomic_flag.html
*/

#pragma once

#include <atomic>
#include <version>
#include <thread>
#include <concurrent/utils/nop_wait.hpp>

namespace concurrent {
 
    /**
     * This is a simple mutex implementation based on atomic_flag.
     * This implementation is adapted from the implementation present at
     * https://en.cppreference.com/w/cpp/atomic/atomic_flag.html
     */
	class AtomicFlagSpinlock final {
		std::atomic_flag atomic_flag{};
	public:

        /**
         * Acquires the lock, blocking or waiting if necessary.
         */
		void lock() noexcept {
            // try to acquire the lock few times
            constexpr u64 SPIN_TRIES = 64;
            for (u64 i = 0; i < SPIN_TRIES; i++) {
                if (!atomic_flag.test_and_set(std::memory_order_acquire)) {
                    return;
                }
                concurrent::nopWait(u64(4) << i);
            }

            while (atomic_flag.test_and_set(std::memory_order_acquire)) {
                std::this_thread::yield();
                // concurrent::nopWait(wait_cycles);
                // if (wait_cycles < 64) {
                //     wait_cycles *= 2;
                // }
                // if (wait_cycles == 128) [[unlikely]] {
                //     std::this_thread::yield();
                //     // we waited for too long, sleep for a bit to lower contention and free CPU:
                //     // wait_cycles = 4;
                // }
            }
		}

        // /**
        //  * Tries to acquire the lock without blocking.
        //  * @return true if the lock was acquired, false otherwise.
        //  */
		// TryLockResult tryLock() noexcept {
        //     auto res = !atomic_flag.test_and_set(std::memory_order_acquire);
        //     return res ? TryLockResult::Acquired : TryLockResult::NotAcquired;
        // }

        /**
         * Releases the lock.
         */
		void unlock() noexcept {
			atomic_flag.clear(std::memory_order_release);
		}
	};
}
