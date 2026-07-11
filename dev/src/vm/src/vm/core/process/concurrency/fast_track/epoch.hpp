#pragma once

/**
 * @file epoch.hpp
 * @brief Epoch representation for the FastTrack algorithm.
 */

#include <base/types/ints.hpp>

#include <vm/api/data/thread_id.hpp>

namespace vm {
	class VectorClock;

	/**
	 * @brief An epoch of the FastTrack algorithm, written c@t in the literature: the value c of
	 * thread t's clock at the time of an access.
	 */
	class Epoch final {
	public:
		/**
		 * @brief Clocks only advance on synchronization events (a lock release, a fork), so 64
		 * bits cannot wrap in practice; a narrower clock would silently invert happens-before
		 * once it overflowed.
		 */
		using Clock = u64;

		/**
		 * @brief The initial epoch, null@bad. It happens-before every access, so a location that
		 * holds it has not been accessed yet.
		 */
		Epoch(): thread_id(api::ThreadID::bad()), clock_value(0) {}

		/**
		 * @brief Construct epoch c@t.
		 */
		constexpr Epoch(api::ThreadID tid, Clock clock): thread_id(tid), clock_value(clock) {}

		/**
		 * @brief Component t from c@t.
		 */
		[[nodiscard]] constexpr api::ThreadID tid() const { return thread_id; }

		/**
		 * @brief Component c from c@t.
		 */
		[[nodiscard]] constexpr Clock clock() const { return clock_value; }

		/**
		 * @brief Epoch comparison with VC: c@t <= VC <=> c <= VC(t).
		 */
		[[nodiscard]] bool operator<=(const VectorClock& vc) const;

		/**
		 * @brief Epoch comparison: c@t <= c'@t' <=> t = t' and c <= c'.
		 */
		[[nodiscard]] constexpr bool operator<=(const Epoch& other) const {
			return thread_id == other.thread_id && clock_value <= other.clock_value;
		}

		/**
		 * @brief Equality: c@t = c'@t' <=> t = t' and c = c'.
		 */
		[[nodiscard]] constexpr bool operator==(const Epoch& other) const {
			return thread_id == other.thread_id && clock_value == other.clock_value;
		}

	private:
		api::ThreadID thread_id;
		Clock         clock_value;
	};
}
