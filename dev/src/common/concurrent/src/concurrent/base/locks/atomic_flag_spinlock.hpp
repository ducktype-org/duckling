// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <concurrent/utils/nop_wait.hpp>

#include <atomic>
#include <thread>
#include <version>

namespace concurrent {

	/**
	 * This is a simple spin/yield-lock implementation based on atomic_flag.
	 * This implementation is partially adapted from the implementation present at
	 * https://en.cppreference.com/w/cpp/atomic/atomic_flag.html
	 *
	 * The behavior of this lock is as follows:
	 * Locking:
	 * - Fast path: a single test_and_set.
	 * - If the lock is taken, it waits with plain loads (test-and-test-and-set) until the lock
	 *   looks free, and only then retries test_and_set. Waiting on a load keeps the cache line
	 *   shared instead of pulling it exclusively to this core on every attempt.
	 * - The waiting spins with increasing amounts of pause instructions. If the lock is still
	 *   taken after a number of spins, the thread yields the CPU before retrying.
	 *
	 * Unlocking:
	 * - The lock is released by clearing the atomic_flag. It is always a wait-free operation.
	 *
	 * @note Waiting used to call sleep_for(50ns), which in practice sleeps ~50us (the kernel
	 * rounds timers up to timer_slack, 50us by default), i.e. ~1000x longer than requested,
	 * while the lock is typically released within nanoseconds.
	 *
	 * @note This lock is not fair at all, and relies to en extend on spin locking which might often
	 * be not ideal. For now it won synthetic tests performed with very fast critical sections
	 * and/or rare waiting (scenarios we expect for example in hash maps).
	 */
	class AtomicFlagSpinlock final {
		std::atomic_flag atomic_flag{};


	public:
		/**
		 * Acquires the lock, spinning and/or yielding if necessary.
		 */
		void lock() noexcept {
			if (!atomic_flag.test_and_set(std::memory_order_acquire)) return;
			u64 wait_repetitions = 2;
			while (true) {
				// test-and-test-and-set: wait on a load (no exclusive cache line ownership needed)
				while (atomic_flag.test(std::memory_order_relaxed)) {
					wait_repetitions *= 2;

					if (wait_repetitions > 128) {
						std::this_thread::yield();
						wait_repetitions = 4;
					} else {
						concurrent::nopWait(wait_repetitions);
					}
				}
				if (!atomic_flag.test_and_set(std::memory_order_acquire)) return;
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
