// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once


#include <base/types/ints.hpp>

#include <vm/api/data/thread_id.hpp>

#include <unordered_map>

namespace vm {

	/**
	 * @brief Detects potential deadlocks in mutex acquisition by tracking wait-for relationships.
	 *
	 * This class implements deadlock detection by maintaining a resource allocation graph where
	 * edges represent "thread waits for mutex" relationships. It detects cycles in this graph that
	 * would indicate deadlock conditions.
	 *
	 * @note Scope: covers mutex wait-for cycles only. Deadlocks involving condition variable
	 *       waits or thread joins are not tracked — std::condition_variable has spurious wakeups,
	 *       making reliable CV-wait edge tracking infeasible.
	 *
	 * @note Thread-Safety: This class has no internal synchronization (no locks). It is designed
	 * to be used exclusively within the context of DVM bytecode execution, where access is
	 * protected by the process Global Interpreter Lock (GIL). Callers must ensure the GIL is
	 * held during all method calls. Concurrent access without the GIL is unsafe.
	 */
	class DeadlockDetector final {
	public:
		/**
		 * @brief Checks if acquiring mutex with id `mutex_id` by thread `thread_id` would cause a
		 * deadlock. If it would, this function should throw an exception to prevent deadlock from
		 * happening.
		 */
		void checkForDeadlock(api::ThreadID thread_id, usize mutex_id);

		void markThreadWaitingForMutex(api::ThreadID thread_id, usize mutex_id);

		void markThreadStoppedWaiting(api::ThreadID thread_id);

		void markThreadAcquiredMutex(api::ThreadID thread_id, usize mutex_id);

		void markThreadReleasedMutex(api::ThreadID thread_id, usize mutex_id);

		/**
		 * @brief Clears detector state related to a destroyed mutex.
		 */
		void clearMutexState(usize mutex_id);

	private:
		// thread_id -> mutex_id (Thread is waiting for Mutex)
		std::unordered_map<api::ThreadID, usize> thread_waiting_for_mutex;

		// mutex_id -> thread_id (Mutex is held by Thread)
		std::unordered_map<usize, api::ThreadID> mutex_owners;
	};
}
