// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/ref.hpp>

#include <atomic>
#include <chrono>

namespace timer {
	using TimeStamp = decltype(std::chrono::high_resolution_clock::now());

	/**
	 * Duration type used for measuring time intervals.
	 * This is just a simple wrapper around std::chrono::nanoseconds,
	 * that adds default zero-initialization, to avoid accidental UB.
	 * Intended use case is to just access the `value` field directly when needed.
	 */
	struct Duration final {
		/**
		 * @note Can be changed to more precise type if needed (e.g. std::chrono::duration<long
		 * double, std::milli>).
		 */
		using DurationValueT = std::chrono::nanoseconds;

		DurationValueT value = DurationValueT::zero();

		static Duration zero() { return Duration{}; }

		[[nodiscard]]
		i64 toNanoseconds() const;

		[[nodiscard]]
		constexpr auto count() const {
			return value.count();
		}
	};

	/**
	 * Atomic variant of Duration, for use in concurrent scenarios.
	 *
	 * All operations on AtomicDuration are thread-safe.
	 * They use relaxed memory order, as we don't require strong ordering guarantees for time
	 * statistics collection.
	 */
	class AtomicDuration final {
		std::atomic<i64> nanoseconds{ 0 };

	public:
		AtomicDuration() = default;

		/**
		 * Construct AtomicDuration from a Duration.
		 */
		AtomicDuration(const Duration& duration);

		/**
		 * Convert AtomicDuration to a Duration.
		 */
		[[nodiscard]]
		Duration toDuration() const;

		/**
		 * Add a Duration to this AtomicDuration.
		 */
		void add(const Duration& duration);
	};

	enum class TimeUnit { Nanoseconds, Microseconds, Milliseconds, Seconds };

	/**
	 * Print duration to the given output stream in the specified time unit.
	 */
	void printAs(std::ostream& out, const Duration& duration, TimeUnit unit);

	/**
	 * Get current high-resolution timestamp.
	 * Inline for performance -- to not impact measured code.
	 */
	inline TimeStamp now() { return std::chrono::high_resolution_clock::now(); }

	/**
	 * Get duration in nanoseconds between two timestamps.
	 * Inline for performance -- to not impact measured code.
	 */
	inline Duration duration(const TimeStamp& start, const TimeStamp& end) {
		return Duration{ std::chrono::duration_cast<Duration::DurationValueT>(end - start) };
	}

	/**
	 * RAII-like object to add time to a given Duration variable.
	 * Measures time from construction to destruction and adds it to the given Duration reference.
	 */
	struct AddToTime final {
		AddToTime(Ref<Duration> to_add): to_add(to_add), start(timer::now()) {}

		~AddToTime() { to_add->value += timer::duration(start, timer::now()).value; }

	private:
		/**
		 * Pointer to the duration to which the time will be added.
		 */
		Ref<Duration> to_add;

		TimeStamp start;
	};

	/**
	 * RAII-like object to add time to a given AtomicDuration variable.
	 * Measures time from construction to destruction and adds it to the given AtomicDuration
	 * reference.
	 */
	struct AddToTimeAtomic final {
		AddToTimeAtomic(Ref<AtomicDuration> to_add): to_add(to_add), start(timer::now()) {}

		~AddToTimeAtomic() { to_add->add(timer::duration(start, timer::now())); }

	private:
		/**
		 * Pointer to the duration to which the time will be added.
		 */
		Ref<AtomicDuration> to_add;

		TimeStamp start;
	};

	/**
	 * Simple utility type to measure time in a typical scenario.
	 * Measures time between startMeasurement() and endMeasurement() calls.
	 */
	struct TimeMeasurement final {
		void startMeasurement() {
			CORE_ASSERT(state == State::NotStarted, "Measurement already started or ended");
			state = State::Started;
			start = timer::now();
		}

		void endMeasurement() {
			CORE_ASSERT(state == State::Started, "Measurement not started or already ended");
			state = State::Ended;
			end   = timer::now();
		}

		/**
		 * Reset the measurement to its initial state.
		 */
		void reset() { state = State::NotStarted; }

		[[nodiscard]]
		Duration duration() const {
			CORE_ASSERT(state == State::Ended, "Measurement not ended");
			return timer::duration(start, end);
		}

	private:
		enum class State { NotStarted, Started, Ended };

		State     state = State::NotStarted;
		TimeStamp start;
		TimeStamp end;
	};
}
