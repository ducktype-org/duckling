/**
 * This file contains a simple spin lock implementation based on atomic_flag.
 * This implementation is adapted from the implementation present at
 * https://en.cppreference.com/w/cpp/atomic/atomic_flag.html
 */

#pragma once

#include <concurrent/utils/nop_wait.hpp>

#include <atomic>
#include <thread>
#include <version>

namespace concurrent {

	/**
	 * This is a simple spin/wait-lock implementation based on atomic_flag.
	 * This implementation is adapted from the implementation present at
	 * https://en.cppreference.com/w/cpp/atomic/atomic_flag.html
	 *
	 * The behavior of this lock is as follows:
	 * Locking:
	 * - It first tries to acquire the lock in a busy-wait loop for a number
	 *   of iterations, using exponential backoff with nop instructions to reduce
	 *   contention.
	 *
	 * - If the lock is not acquired in the busy-wait phase, it then tries to
	 *   acquire the lock by yielding the thread, allowing other threads to run.
	 *   This phase uses `std::this_thread::yield()` which proved to be more effective
	 *   than sleep for short waits.
	 *
	 * - If the lock is still not acquired after yielding, it falls back to
	 *   sleeping for a short duration in a loop until the lock is acquired.
	 *   This phase is only present due to the lack of guarantees about the behavior
	 *   of `std::this_thread::yield()`.
	 *
	 * Unlocking:
	 * - The lock is released by clearing the atomic_flag. It is always a wait-free operation.
	 *
	 * @note This lock is not fair at all, and relies to en extend on spin locking which might not
	 * always be ideal. However, it is very simple and efficient for short critical sections. With
	 * experimental testing performed so far it proven much faster than mutexes/semaphores,
	 *       `std::atomic_flag.wait()` based synchronization, or other more complex lock
	 * implementations. It is especially effective for short critical sections or when waiting for
	 * lock is rare.
	 */
	class AtomicFlagSpinlock final {
		std::atomic_flag atomic_flag{};


	public:
		/**
		 * Acquires the lock, waiting or sleeping if necessary.
		 */
		void lock() noexcept {
			// try to acquire the lock in a busy wait a few times:

			// note: higher numbers make it less fair, but usually faster
			constexpr u64 SPIN_TRIES = 128;

			u64 wait_rep = 2;
			for (u64 i = 0; i < SPIN_TRIES; i++) {
				if (!atomic_flag.test_and_set(std::memory_order_acquire)) return;

				// note: higher nopWait counts make it less fair, but usually faster
				wait_rep *= 2;
				wait_rep = std::min(wait_rep, u64(2'048));
				concurrent::nopWait(wait_rep);
			}

			// if not successful, yield until the lock is acquired
			// we yield a lot of times here, as we only want to sleep in extreme cases:
			for (u64 i = 0; i < SPIN_TRIES * 16; i++) {
				if (!atomic_flag.test_and_set(std::memory_order_acquire)) return;
				std::this_thread::yield();
			}

			// yield might technically not do anything (no guarantees by the standard),
			// so after some tries, we wait with sleep:
			while (atomic_flag.test_and_set(std::memory_order_acquire))
				std::this_thread::sleep_for(std::chrono::nanoseconds(50));
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
		void unlock() noexcept { atomic_flag.clear(std::memory_order_release); }
	};
}
