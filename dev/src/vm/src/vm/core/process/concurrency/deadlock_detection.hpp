#pragma once


#include <base/types/ints.hpp>
#include <map>
#include <vector>
#include <thread>

namespace vm {

    class DeadlockDetector final {
    public:
        /**
         * @brief Checks if acquiring mutex with id `mutex_id` by thread `thread_id` would cause a deadlock.
         * If it would, this function should throw an exception to prevent deadlock from happening.
         */
        static void checkForDeadlock(std::thread::id thread_id, usize mutex_id);

        static void markThreadWaitingForMutex(std::thread::id thread_id, usize mutex_id);

        static void markThreadAcquiredMutex(std::thread::id thread_id, usize mutex_id);

        static void markThreadReleasedMutex(std::thread::id thread_id, usize mutex_id);

    private:
        // thread_id -> mutex_id (Thread is waiting for Mutex)
        static std::map<std::thread::id, usize> thread_waiting_for_mutex; 

        // mutex_id -> thread_id (Mutex is held by Thread)
        // thread_holding_mutex was renamed to mutex_owners to correctly represent the relation (mutex -> thread)
        // This supports threads holding multiple mutexes.
        static std::map<usize, std::thread::id> mutex_owners; 
    };
}