#pragma once


#include <base/types/ints.hpp>

#include <vm/api/data/thread_id.hpp>

#include <map>

namespace vm {

	/**
	 * @brief Detects potential deadlocks in mutex acquisition by tracking wait-for relationships.
	 *
	 * This class implements deadlock detection by maintaining a resource allocation graph where
	 * edges represent "thread waits for mutex" relationships. It detects cycles in this graph that
	 * would indicate deadlock conditions.
	 *
	 * @note Thread-Safety: This class has no internal synchronization (no locks). It is designed
	 * to be used exclusively within the context of DVM bytecode execution, where access is
	 * protected by the process Global Interpreter Lock (GIL). Callers must ensure the GIL is
	 * held during all method calls. Concurrent access without the GIL is unsafe.
	 */
	class DeadlockDetector final {
	public:
		/**
		 * @brief Checks if waiting for `mutex_id` would deadlock and marks thread as waiting.
		 *
		 * This method performs two operations: (1) checking for deadlock, and (2) marking the
		 * thread as waiting. While these are logically atomic from the DVM bytecode perspective,
		 * the class itself has no internal synchronization. Thread-safety is provided by the
		 * caller's responsibility to hold the process Global Interpreter Lock (GIL) during
		 * bytecode execution. See VMProcess::getGIL().
		 *
		 * @throws VMDeadlockException if the mutex acquisition would cause a deadlock.
		 */
		void beginWaitForMutexOrThrow(api::ThreadID thread_id, usize mutex_id);

		/**
		 * @brief Checks if acquiring mutex with id `mutex_id` by thread `thread_id` would cause a
		 * deadlock. If it would, this function should throw an exception to prevent deadlock from
		 * happening.
		 */
		void checkForDeadlock(api::ThreadID thread_id, usize mutex_id);

		void markThreadWaitingForMutex(api::ThreadID thread_id, usize mutex_id);

		void markThreadAcquiredMutex(api::ThreadID thread_id, usize mutex_id);

		void markThreadReleasedMutex(api::ThreadID thread_id, usize mutex_id);

		/**
		 * @brief Clears detector state related to a destroyed mutex.
		 */
		void clearMutexState(usize mutex_id);

	private:
		// thread_id -> mutex_id (Thread is waiting for Mutex)
		std::map<api::ThreadID, usize> thread_waiting_for_mutex;

		// mutex_id -> thread_id (Mutex is held by Thread)
		std::map<usize, api::ThreadID> mutex_owners;
	};
}
