#pragma once

#include <base/pointers/ref.hpp>

namespace concurrent {
    /**
     * RAII wrapper for locking a lock of type LockType.
     * It is an analogy of std::lock_guard, that can be safely used with out locks.
     */
    template<typename LockType>
    struct WithLock final {
    private:
        CRef<LockType> lock;
    public:
        WithLock(CRef<LockType> lock): lock(lock) {
            this->lock->lock();
        }

        ~WithLock() {
            this->lock->unlock();
        }
    };

    template<typename LockType>
    WithLock(CRef<LockType> lock) -> WithLock<LockType>;
    
    template<typename LockType>
    WithLock(const LockType* lock) -> WithLock<LockType>;
}
