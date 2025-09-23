#pragma once

#include "atomic_u64.hpp"
#include "nop_wait.hpp"

namespace concurrent {

    /**
     * A lock that allows multiple readers or one writer.
     * Based on active waiting.
     * Is not fair at all, usefull in cases where writers are rare, and both reads and writes are very fast,
     * (i.e. fast enough that context switching is more expensive then just waiting).
     *
     * - acquireRead() and releaseRead() have to be called in pairs (not necessarily from the same thread/worker).
     * - acquireWrite() and releaseWrite() have to be called in pairs (not necessarily from the same thread/worker).
     *
     * Assumes that there are less then 2^62 readers at the same time.
     */
    struct RWSpinLock final {
    private:

        /**
         * RWLock state layout:
         * - lower 62 bits -- number of readers holding the lock
         * - bit 62 -- writer blocked. Set when a writer is waiting for the lock or when it is already held.
         */
        AtomicU64 state = 0;

        constexpr static u64 READER_MASK = (1ull << 62) - 1;
        constexpr static u64 WRITER_HELD_MASK = 1ull << 62;
        // constexpr static u64 WRITER_HELD_MASK = 1ull << 63;
    public:
        void acquireRead() {
            while (true) {
                u64 s = state.load();

                // if there is a writer waiting or holding the lock, wait
                if ((s & WRITER_HELD_MASK) != 0) {
                    nopWait(2);
                    continue;
                }
                
                // try to increment the reader count
                if (state.cmpAndSwap(s, s + 1) == CmpRes::Changed) {
                    return;
                }
            }
        }

        void releaseRead() {
            state.dec();
        }

        void acquireWrite() {
            // Indicate that a writer is waiting
            while (true) {
                u64 s = state.load();
                if ((s & WRITER_HELD_MASK) != 0) {
                    // another writer is already waiting, just wait
                    nopWait(2);
                } else {
                    // try to set the writer waiting flag
                    if (state.cmpAndSwap(s, s | WRITER_HELD_MASK) == CmpRes::Changed) {
                        break;
                    }
                }
            }

            // we now have the writer flag set
            // we just need to wait for readers to finish

            while (true) {
                u64 s = state.load();
                if (s == WRITER_HELD_MASK) {
                    // no readers present, we can take the lock
                        return;
                } else {
                    // readers are still active, wait
                    nopWait(1);
                }
            }
        }

        void releaseWrite() {
            state.sub(WRITER_HELD_MASK);
        }

    };
}