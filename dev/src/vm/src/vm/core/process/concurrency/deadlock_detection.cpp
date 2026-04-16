#include "deadlock_detection.hpp"

#include <vm/core/process/exceptions.hpp>

#include <vector>

namespace vm {

	void DeadlockDetector::beginWaitForMutexOrThrow(api::ThreadID thread_id, usize mutex_id) {
		// This method's thread-safety relies on caller holding the process GIL.
		// The two operations below form a logical atomic unit within bytecode execution:
		// 1. Check if the acquisition would cause a deadlock
		// 2. Mark thread as waiting (before actually acquiring the OS-level mutex)
		// If a deadlock is detected, an exception is thrown and the thread is NOT marked as waiting.
		checkForDeadlock(thread_id, mutex_id);
		markThreadWaitingForMutex(thread_id, mutex_id);
	}

	void DeadlockDetector::checkForDeadlock(api::ThreadID thread_id, usize mutex_id) {
		// Thread-safety: No internal locking. Assumes caller holds the process GIL.
		// Algorithm: Detects cycles in the wait-for graph using DFS.
		// Check if there is a cycle if we add edge thread_id -> mutex owner

		// If the mutex is not owned by anyone, no deadlock possible from this acquisition
		auto owner_it = mutex_owners.find(mutex_id);
		if (owner_it == mutex_owners.end()) return;

		api::ThreadID owner_thread_id = owner_it->second;

		// Self-deadlock: std::mutex is non-recursive. Locking it again deadlocks.
		if (owner_thread_id == thread_id) throw exceptions::VMDeadlockException();

		// DFS to find if owner_thread_id can reach thread_id in the wait-for graph.
		// If such a path exists, a cycle would be created (deadlock condition).
		// Graph edges: Thread A waiting for Mutex M owned by Thread B => Edge A -> B
		// We are adding edge (thread_id -> owner_thread_id) and checking if it creates a cycle
		// by searching for a path from owner_thread_id back to thread_id.

		std::vector<bool>          visited;
		std::vector<api::ThreadID> stack;

		auto is_visited = [&](api::ThreadID id) {
			auto index = static_cast<usize>(id.asInt());
			return index < visited.size() && visited[index];
		};

		auto mark_visited = [&](api::ThreadID id) {
			auto index = static_cast<usize>(id.asInt());
			if (index >= visited.size()) visited.resize(index + 1, false);
			visited[index] = true;
		};

		stack.push_back(owner_thread_id);
		mark_visited(owner_thread_id);

		while (!stack.empty()) {
			api::ThreadID current_thread = stack.back();
			stack.pop_back();

			// If we reached the original thread_id, a cycle exists -> deadlock
			if (current_thread == thread_id) throw exceptions::VMDeadlockException();

			// Traverse the wait-for graph: find what mutex current_thread is waiting for,
			// then find who owns that mutex (next thread to visit).
			auto wait_it = thread_waiting_for_mutex.find(current_thread);
			if (wait_it != thread_waiting_for_mutex.end()) {
				usize waiting_for_mutex = wait_it->second;

				auto next_owner_it = mutex_owners.find(waiting_for_mutex);
				if (next_owner_it != mutex_owners.end()) {
					api::ThreadID next_thread = next_owner_it->second;

					// Cycle found: current thread is waiting for a mutex ultimately held by thread_id
					if (next_thread == thread_id) throw exceptions::VMDeadlockException();

					if (!is_visited(next_thread)) {
						mark_visited(next_thread);
						stack.push_back(next_thread);
					}
				}
			}
		}
	}

	void DeadlockDetector::markThreadWaitingForMutex(api::ThreadID thread_id, usize mutex_id) {
		thread_waiting_for_mutex[thread_id] = mutex_id;
	}

	void DeadlockDetector::markThreadNoLongerWaiting(api::ThreadID thread_id) {
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

		for (auto it = thread_waiting_for_mutex.begin(); it != thread_waiting_for_mutex.end();)
			if (it->second == mutex_id)
				it = thread_waiting_for_mutex.erase(it);
			else
				++it;
	}

}
