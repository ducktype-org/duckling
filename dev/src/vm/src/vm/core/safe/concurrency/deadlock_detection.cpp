#include "deadlock_detection.hpp"

#include <vm/core/safe/exceptions.hpp>

namespace vm {

	void DeadlockDetector::checkForDeadlock(api::ThreadID thread_id, usize mutex_id) {
		// Thread-safety: No internal locking. Assumes caller holds the process GIL.
		//
		// The wait-for graph is a functional graph: each thread waits for at most one mutex,
		// each mutex is owned by at most one thread. Combined, each thread has out-degree <= 1.
		//
		// Invariant: the existing graph is always acyclic (maintained by this very check).
		// Therefore, following the chain is guaranteed to terminate — no visited tracking needed.
		//
		// We check: if we add edge (thread_id -> owner_of_mutex_id), does that create a cycle?
		// Equivalently: is there already a path from owner_of_mutex_id back to thread_id?

		auto owner_it = mutex_owners.find(mutex_id);
		if (owner_it == mutex_owners.end()) return;

		api::ThreadID current = owner_it->second;

		while (true) {
			if (current == thread_id) throw exceptions::VMDeadlockException();

			auto wait_it = thread_waiting_for_mutex.find(current);
			if (wait_it == thread_waiting_for_mutex.end()) return;

			auto next_owner_it = mutex_owners.find(wait_it->second);
			if (next_owner_it == mutex_owners.end()) return;

			current = next_owner_it->second;
		}
	}

	void DeadlockDetector::markThreadWaitingForMutex(api::ThreadID thread_id, usize mutex_id) {
		thread_waiting_for_mutex[thread_id] = mutex_id;
	}

	void DeadlockDetector::markThreadStoppedWaiting(api::ThreadID thread_id) {
		thread_waiting_for_mutex.erase(thread_id);
	}

	void DeadlockDetector::markThreadAcquiredMutex(api::ThreadID thread_id, usize mutex_id) {
		thread_waiting_for_mutex.erase(thread_id);
		mutex_owners[mutex_id] = thread_id;
	}

	void DeadlockDetector::markThreadReleasedMutex(api::ThreadID thread_id, usize mutex_id) {
		auto it = mutex_owners.find(mutex_id);
		if (it != mutex_owners.end() && it->second == thread_id) mutex_owners.erase(it);
	}

	void DeadlockDetector::clearMutexState(usize mutex_id) {
		mutex_owners.erase(mutex_id);
		for (auto it = thread_waiting_for_mutex.begin(); it != thread_waiting_for_mutex.end();) {
			if (it->second == mutex_id) it = thread_waiting_for_mutex.erase(it);
			else ++it;
		}
	}

}
