#pragma once

#include <base/ref.hpp>

#include <chrono>

namespace timer {
	using TimeStamp = decltype(std::chrono::high_resolution_clock::now());
	using Duration  = std::chrono::nanoseconds;

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
		return std::chrono::duration_cast<Duration>(end - start);
	}

	/**
	 * RAII-like object to add time to a given Duration variable.
	 */
	struct AddToTime final {
		AddToTime(Ref<Duration> to_add): to_add(to_add), start(now()) {}

		~AddToTime() { *to_add += timer::duration(start, now()); }

	private:
		/**
		 * Pointer to the duration to which the time will be added.
		 */
		Ref<Duration> to_add;

		TimeStamp start;
	};

	/**
	 * Simple utility type to measure time in a typical scenario.
	 * The time from start to end.
	 */
	struct TimeMeasurement final {
		void startMeasurement() {
			CORE_ASSERT(state == NotStarted, "Measurement already started or ended");
			state = Started;
			start = timer::now();
		}

		void endMeasurement() {
			CORE_ASSERT(state == Started, "Measurement not started or already ended");
			state = Ended;
			end   = timer::now();
		}

		[[nodiscard]]
		Duration duration() const {
			CORE_ASSERT(state == Ended, "Measurement not ended");
			return timer::duration(start, end);
		}

	private:
		enum State { NotStarted, Started, Ended };

		State     state = NotStarted;
		TimeStamp start;
		TimeStamp end;
	};
}
