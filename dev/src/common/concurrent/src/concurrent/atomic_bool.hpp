#pragma once

#include <atomic>
#include <base/ints.hpp>
#include "utils.hpp"

namespace concurrent {

    struct AtomicBool {
    private:
        // test: see what using atomic flag gives
        std::atomic<bool> value;

    public:
        AtomicBool(bool value) : value(value) {}

        bool load() {
            return value.load();
        }
        void store(bool desired) {
            value.store(desired);
        }
        CmpRes cmpAndSwap(bool expected, bool desired) {
            bool res = value.compare_exchange_strong(expected, desired);
            return res ? CmpRes::Changed : CmpRes::NotChanged;
        }
    };

}