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
        static void checkForDeadlock(u64 process_id, std::thread::id thread_id, usize mutex_id);

        static void markThreadWaitingForMutex(u64 process_id, std::thread::id thread_id, usize mutex_id);

        static void markThreadAcquiredMutex(u64 process_id, std::thread::id thread_id, usize mutex_id);

        static void markThreadReleasedMutex(u64 process_id, std::thread::id thread_id, usize mutex_id);

    private:
        // (process_id, thread_id) -> mutex_id (Thread is waiting for Mutex)
        static std::map<std::pair<u64, std::thread::id>, usize> thread_waiting_for_mutex;

        // (process_id, mutex_id) -> thread_id (Mutex is held by Thread)
        static std::map<std::pair<u64, usize>, std::thread::id> mutex_owners;
    };
}