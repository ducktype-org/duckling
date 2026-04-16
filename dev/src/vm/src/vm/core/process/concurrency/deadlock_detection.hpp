#pragma once


#include <base/types/ints.hpp>
#include <vm/api/data/thread_id.hpp>
#include <map>

namespace vm {

    class DeadlockDetector final {
    public:
        /**
         * @brief Atomically checks if waiting for `mutex_id` would deadlock and marks thread as waiting.
         */
        void beginWaitForMutexOrThrow(api::ThreadID thread_id, usize mutex_id);

        /**
         * @brief Checks if acquiring mutex with id `mutex_id` by thread `thread_id` would cause a deadlock.
         * If it would, this function should throw an exception to prevent deadlock from happening.
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