// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "timer.hpp"

namespace timer {

	i64 Duration::toNanoseconds() const {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(value).count();
	}

	AtomicDuration::AtomicDuration(const Duration& duration):
		  nanoseconds(duration.toNanoseconds()) {}

	Duration AtomicDuration::toDuration() const {
		return Duration{ std::chrono::nanoseconds(nanoseconds.load(std::memory_order_relaxed)) };
	}

	void AtomicDuration::add(const Duration& duration) {
		nanoseconds.fetch_add(duration.toNanoseconds(), std::memory_order_relaxed);
	}

	void printAs(std::ostream& out, const Duration& duration, TimeUnit unit) {
		switch (unit) {
		case TimeUnit::Nanoseconds:
			out << std::chrono::duration_cast<std::chrono::nanoseconds>(duration.value).count()
				<< " ns";
			break;
		case TimeUnit::Microseconds:
			out << std::chrono::duration_cast<std::chrono::microseconds>(duration.value).count()
				<< " µs";
			break;
		case TimeUnit::Milliseconds:
			out << std::chrono::duration_cast<std::chrono::milliseconds>(duration.value).count()
				<< " ms";
			break;
		case TimeUnit::Seconds:
			out << std::chrono::duration_cast<std::chrono::seconds>(duration.value).count() << " s";
			break;
		}
	}
}
