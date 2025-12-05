#pragma once

#include <base/types/ints.hpp>

namespace concurrent {
    /**
     * Sets the (max) number of workers (i.e. threads) that will be used.
     * This function can only be called once and must be called before
     * most other functionaries of the concurrent module are used. 
     */
    void setWorkerCount(u64 value);

    /**
     * Gets the (max) number of workers (i.e. threads) that will be used.
     * This function can only be called after setWorkerCount has been called.
     */
    u64 getWorkerCount();
}
