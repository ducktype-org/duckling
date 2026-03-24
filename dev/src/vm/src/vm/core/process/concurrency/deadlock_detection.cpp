#include "deadlock_detection.hpp"


#include <stdexcept>
#include <vm/core/process/exceptions.hpp>
#include <set>
#include <vector>

namespace vm {

    std::map<std::thread::id, usize> DeadlockDetector::thread_waiting_for_mutex;
    std::map<usize, std::thread::id> DeadlockDetector::mutex_owners;

    void DeadlockDetector::checkForDeadlock(std::thread::id thread_id, usize mutex_id) {
        // Check if there is a cycle in resource allocation graph if we add edge thread_id -> mutex_id
        
        // If the mutex is not owned by anyone, no deadlock possible from this acquisition
        auto owner_it = mutex_owners.find(mutex_id);
        if (owner_it == mutex_owners.end()) {
            return;
        }

        std::thread::id owner_thread_id = owner_it->second;

        // If the thread already owns the mutex, it's a recursive lock.
        if (owner_thread_id == thread_id) {
            return; // Recursive locks are allowed, we can just return here.
        }

        // DFS to find if owner_thread_id can reach thread_id in the wait-for graph
        // Graph edges: Thread A waiting for Mutex M (owned by Thread B) => A -> B
        // We are checking edge thread_id -> owner_thread_id.
        // So we want to see if there is path owner_thread_id -> ... -> thread_id.

        std::set<std::thread::id> visited;
        std::vector<std::thread::id> stack;
        
        stack.push_back(owner_thread_id);
        visited.insert(owner_thread_id);

        while (!stack.empty()) {
            std::thread::id current_thread = stack.back();
            stack.pop_back();

            if (current_thread == thread_id) {
                throw exceptions::VMDeadlockException();
            }

            // Find what mutex current_thread is waiting for
            auto wait_it = thread_waiting_for_mutex.find(current_thread);
            if (wait_it != thread_waiting_for_mutex.end()) {
                usize waiting_for_mutex = wait_it->second;
                
                // Find who owns that mutex
                auto next_owner_it = mutex_owners.find(waiting_for_mutex);
                if (next_owner_it != mutex_owners.end()) {
                    std::thread::id next_thread = next_owner_it->second;
                    
                    if (visited.find(next_thread) == visited.end()) {
                        visited.insert(next_thread);
                        stack.push_back(next_thread);
                    } else if (next_thread == thread_id) {
                        // Found cycle back to thread_id
                         throw exceptions::VMDeadlockException();
                    }
                }
            }
        }
    }

    void DeadlockDetector::markThreadWaitingForMutex(std::thread::id thread_id, usize mutex_id) {
        thread_waiting_for_mutex[thread_id] = mutex_id;
    }

    void DeadlockDetector::markThreadAcquiredMutex(std::thread::id thread_id, usize mutex_id) {
        thread_waiting_for_mutex.erase(thread_id);
        mutex_owners[mutex_id] = thread_id;
    }

    void DeadlockDetector::markThreadReleasedMutex(std::thread::id thread_id, usize mutex_id) {
        auto it = mutex_owners.find(mutex_id);
        if (it != mutex_owners.end() && it->second == thread_id) {
            mutex_owners.erase(it);
        }
    }

}
