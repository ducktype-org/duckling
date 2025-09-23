#pragma once

#include "atomic_bool.hpp"

namespace concurrent {

    /**
     * A simple spin lock based on active waiting.
     * - acquire() and release() have to be called in pairs (not necessarily from the same thread/worker).
     */
    struct SpinLock final {
    private:
        AtomicBool state;
    public:
        void acquire() {
            while (true) {
                // try to acquire the lock
                if (state.cmpAndSwap(false, true) == CmpRes::Changed)
                    return;
            }
        }

        void release() {
            state.store(false);
        }
    };
}