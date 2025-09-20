#pragma once

#include <chrono>

namespace timer {
    using TimeStamp = decltype(std::chrono::high_resolution_clock::now());
    using Duration =  std::chrono::nanoseconds;

    enum class TimeUnit {
        Nanoseconds,
        Microseconds,
        Milliseconds,
        Seconds
    };

    /**
     * Print duration to the given output stream in the specified time unit.
     */
    void printAs(std::ostream& out, const Duration& duration, TimeUnit unit);

    /**
     * Get current high-resolution timestamp.
     * Inline for performance -- to not impact measured code.
     */
    inline TimeStamp now() {
        return std::chrono::high_resolution_clock::now();
    }

    /**
     * Get duration in nanoseconds between two timestamps.
     * Inline for performance -- to not impact measured code.
     */
    inline Duration duration(const TimeStamp& start, const TimeStamp& end) {
        return std::chrono::duration_cast<Duration>(end - start);
    }
}