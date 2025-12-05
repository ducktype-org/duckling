#pragma once

#include <atomic>

namespace concurrent {

    class AtomicFlagMutex final
    {
        std::atomic_flag m_{};
    
    public:
        void lock() noexcept
        {
            while (m_.test_and_set(std::memory_order_acquire))
    #if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
                // Since C++20, locks can be acquired only after notification in the unlock,
                // avoiding any unnecessary spinning.
                // Note that even though wait guarantees it returns only after the value has
                // changed, the lock is acquired after the next condition check.
                m_.wait(true, std::memory_order_relaxed)
    #endif
                    ;
        }
        bool try_lock() noexcept
        {
            return !m_.test_and_set(std::memory_order_acquire);
        }
        void unlock() noexcept
        {
            m_.clear(std::memory_order_release);
    #if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
            m_.notify_one();
    #endif
        }
    };
}