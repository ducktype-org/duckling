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
            // try to acquire the lock in a busy wait a few times:
            // note: higher numbers make it less fair, but faster
            constexpr u64 SPIN_TRIES = 128;

            u64 wait_rep = 2;
            for (u64 i = 0; i < SPIN_TRIES; i++) {
                if (!atomic_flag.test_and_set(std::memory_order_acquire)) {
                    return;
                }

                // note: higher nopWait counts make it less fair, but faster
                wait_rep *= 2;
                wait_rep = std::min(wait_rep, u64(4096));
                concurrent::nopWait(wait_rep);
            }
            
            // if not successful, yield until the lock is acquired
            for (u64 i = 0; i < SPIN_TRIES * 16; i++) {
                if (!atomic_flag.test_and_set(std::memory_order_acquire)) {
                    return;
                }
                std::this_thread::yield();
            }

            // yield might technically not do anything (no guarantees by the standard),
            // so after some tries, we wait with sleep:
            while (atomic_flag.test_and_set(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(50));
            }
		}

        // This is left for reference, but not used currently.
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
