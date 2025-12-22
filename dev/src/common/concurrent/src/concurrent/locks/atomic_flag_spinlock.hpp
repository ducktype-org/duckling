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
	 *   of iterations, increasing the amount of spining with each iteration.
	 *
	 * - If the lock is not acquired in the busy-wait phase, it then tries to
	 *   acquire the lock in the `std::this_thread::yield()` loop, allowing other threads to run.
	 *   This phase uses `std::this_thread::yield()` because it proven to be very efficient
	 *   in short wait scenarios.
	 *
	 * - If the lock is still not acquired after yielding, it falls back to
	 *   sleeping for a short duration in a loop until the lock is acquired.
	 *   This phase is only present mostly due to the lack of guarantees about the behavior
	 *   of `std::this_thread::yield()`.
	 *
	 * Unlocking:
	 * - The lock is released by clearing the atomic_flag. It is always a wait-free operation.
	 *
	 * @note This lock is not fair at all, and relies to en extend on spin locking which might often
	 * be not ideal. For now it won synthetic tests performed with very fast critical sections
	 * and/or rare waiting (scenarios we expect for example in hash maps). In the future, when
	 * possible, it might be worth to benchmark it in real scenarios and potentially swap it /
	 * improve it.
	 */
	class AtomicFlagSpinlock final {
		std::atomic_flag atomic_flag{};


	public:
		/**
		 * Acquires the lock, waiting or sleeping if necessary.
		 */
		void lock() noexcept {
			// try to acquire the lock in a busy wait loop first:
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
			// we yield a lot of times here, as we only want to call `sleep_for` if yielding is
			// somehow unsuccessful:
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
