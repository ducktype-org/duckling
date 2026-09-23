#pragma once

/**
 * @file epoch.hpp
 * @brief Epoch representation for the FastTrack algorithm.
 */

#include <base/except/exceptions.hpp>
#include <base/types/ints.hpp>

#include <vm/api/data/thread_id.hpp>

namespace vm {
	class VectorClock;

	/**
	 * @brief An epoch of the FastTrack algorithm, written c@t in the literature: the value c of
	 * thread t's clock at the time of an access.
	 *
	 * Packed into one word, as in the paper: the thread ID in the upper `TID_BITS` bits and the
	 * clock in the lower `CLOCK_BITS`. Every shadowed location holds two epochs, so the size of
	 * an epoch is what the shadow memory scales with.
	 */
	class Epoch final {
	public:
		using Clock = u64;

		/// How the 64 bits of an epoch are split between the thread ID and the clock.
		static constexpr unsigned TID_BITS   = 24;
		static constexpr unsigned CLOCK_BITS = 64 - TID_BITS;

		/**
		 * @brief The largest thread ID an epoch can name. Thread IDs are slots of the process's
		 * thread pool, so this bounds the number of threads a process may create. The all-ones
		 * pattern is reserved for the bad ID of the initial epoch.
		 */
		static constexpr u64 MAX_TID = (u64(1) << TID_BITS) - 2;

		/**
		 * @brief The largest clock an epoch can hold. Clocks only advance on synchronization
		 * events (a lock release, a fork), so 2^40 of them on one thread is out of reach in
		 * practice. `VectorClock::increment` refuses to go past it rather than wrap: a wrapped
		 * clock would silently invert happens-before.
		 */
		static constexpr Clock MAX_CLOCK = (u64(1) << CLOCK_BITS) - 1;

		/**
		 * @brief The initial epoch, null@bad. It happens-before every access, so a location that
		 * holds it has not been accessed yet.
		 */
		constexpr Epoch(): packed(BAD_TID_BITS << CLOCK_BITS) {}

		/**
		 * @brief Construct epoch c@t. `tid` is either bad or at most `MAX_TID`, and `clock` is at
		 * most `MAX_CLOCK`; `VectorClock` enforces both for every clock value it hands out.
		 */
		constexpr Epoch(api::ThreadID tid, Clock clock): packed(pack(tid, clock)) {}

		/**
		 * @brief Component t from c@t.
		 */
		[[nodiscard]] constexpr api::ThreadID tid() const {
			const u64 tid_bits = packed >> CLOCK_BITS;
			return tid_bits == BAD_TID_BITS ? api::ThreadID{} : api::ThreadID{ tid_bits };
		}

		/**
		 * @brief Component c from c@t.
		 */
		[[nodiscard]] constexpr Clock clock() const { return packed & MAX_CLOCK; }

		/**
		 * @brief Epoch comparison with VC: c@t <= VC <=> c <= VC(t). The initial epoch is ordered
		 * before every VC. There is no epoch-to-epoch order: the read and write rules only ever
		 * compare an epoch with a vector clock.
		 */
		[[nodiscard]] bool operator<=(const VectorClock& vc) const;

		/**
		 * @brief Equality: c@t = c'@t' <=> t = t' and c = c'.
		 */
		[[nodiscard]] constexpr bool operator==(const Epoch& other) const = default;

	private:
		/// The thread ID bits of the initial epoch. `api::ThreadID::bad()` itself does not fit
		/// in `TID_BITS`.
		static constexpr u64 BAD_TID_BITS = (u64(1) << TID_BITS) - 1;

		static constexpr u64 pack(api::ThreadID tid, Clock clock) {
			// `api::ThreadID{}` is the bad ID; `api::ThreadID::bad()` is not constexpr. The check
			// looks at the ID itself: the good ID `2^TID_BITS - 1` would pack as the bad one.
			const bool is_bad = tid.asInt() == api::ThreadID{}.asInt();
			CORE_ASSERT(is_bad || tid.asInt() <= MAX_TID, "Thread ID does not fit in an epoch");
			const u64 tid_bits = is_bad ? BAD_TID_BITS : tid.asInt();
			CORE_ASSERT(clock <= MAX_CLOCK, "Clock does not fit in an epoch");
			return (tid_bits << CLOCK_BITS) | clock;
		}

		u64 packed;
	};

	static_assert(sizeof(Epoch) == sizeof(u64), "An epoch is meant to be a single word");
}
