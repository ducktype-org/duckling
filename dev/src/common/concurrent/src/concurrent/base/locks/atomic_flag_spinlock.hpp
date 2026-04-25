#pragma once

#include <concurrent/utils/nop_wait.hpp>

#include <atomic>
#include <thread>
#include <version>

namespace concurrent {

	/**
	 * This is a simple spin/sleep-lock implementation based on atomic_flag.
	 * This implementation is partially adapted from the implementation present at
	 * https://en.cppreference.com/w/cpp/atomic/atomic_flag.html
	 *
	 * The behavior of this lock is as follows:
	 * Locking:
	 * - It tries to acquire the lock in a busy-wait loop for a number
	 *   of iterations, increasing the amount of spining with each iteration.
	 * - If the lock is not acquired after a certain number of spins, the thread
	 *   sleeps for a short amount of time, before retrying the entire process.
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
		 * Acquires the lock, spinning and/or sleeping if necessary.
		 */
		void lock() noexcept {
			u64 wait_repetitions = 2;
			while (true) {
				if (!atomic_flag.test_and_set(std::memory_order_acquire)) return;
				wait_repetitions *= 2;

				if (wait_repetitions > 128) {
					// std::this_thread::sleep_for(std::chrono::nanoseconds(50));
					// std::this_thread::yield();
					wait_repetitions = 4;
				} else {
					concurrent::nopWait(wait_repetitions);
				}
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
		void unlock() noexcept { atomic_flag.clear(std::memory_order_release); }
	};
}
