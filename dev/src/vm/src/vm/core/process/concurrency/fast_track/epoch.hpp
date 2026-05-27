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
	 * @brief Represents an Epoch in the FastTrack algorithm.
	 * Denoted in literature as c@t (clock c, thread t).
	 */
	class Epoch {
	public:
		using Clock    = i32;
		using ThreadId = api::ThreadID;

		/**
		 * @brief Initial state: null@bad.
		 */
		constexpr Epoch(): thread_id(api::ThreadID::bad()), clock_value(0) {}

		/**
		 * @brief Construct epoch c@t.
		 */
		constexpr Epoch(ThreadId tid, Clock clock): thread_id(tid), clock_value(clock) {}

		/**
		 * @brief Component t from c@t.
		 */
		[[nodiscard]] constexpr ThreadId tid() const { return thread_id; }

		/**
		 * @brief Component c from c@t.
		 */
		[[nodiscard]] constexpr Clock clock() const { return clock_value; }

		/**
		 * @brief Successor: c@t -> (c+1)@t.
		 */
		void increment();

		/**
		 * @brief Epoch comparison with VC: c@t <= VC <=> c <= VC(t).
		 */
		bool operator<=(const VectorClock& vc) const;

		/**
		 * @brief Epoch comparison: c@t <= c'@t' <=> t = t' \land c <= c'.
		 */
		constexpr bool operator<=(const Epoch& other) const {
			return thread_id == other.thread_id && clock_value <= other.clock_value;
		}

		/**
		 * @brief Equality: c@t = c'@t' <=> t = t' \land c = c'.
		 */
		constexpr bool operator==(const Epoch& other) const {
			return thread_id == other.thread_id && clock_value == other.clock_value;
		}

		/**
		 * @brief Inequality: c@t != c'@t'.
		 */
		constexpr bool operator!=(const Epoch& other) const { return !(*this == other); }

	private:
		ThreadId thread_id;
		Clock    clock_value;
	};
}
