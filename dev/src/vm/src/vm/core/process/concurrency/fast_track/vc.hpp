#pragma once

/**
 * @file vc.hpp
 * @brief Vector Clock implementation for the FastTrack algorithm.
 */

#include "epoch.hpp"

#include <base/types/ints.hpp>

#include <vm/api/data/thread_id.hpp>

#include <vector>

namespace vm {
	/**
	 * @brief Vector Clock (VC) used to track the happens-before relationship.
	 * Denoted in literature as V or VC. Components of threads it has never seen read as 0.
	 */
	class VectorClock final {
	private:
		std::vector<Epoch::Clock> clocks;

		/**
		 * @brief Ensures capacity for thread t: |VC| >= t + 1.
		 */
		void ensureCapacity(api::ThreadID thread_id);

	public:
		/**
		 * @brief Default constructor. VC = \lambda t. 0.
		 */
		VectorClock() = default;

		/**
		 * @brief Read component: VC(t). Zero for any thread past the end.
		 */
		[[nodiscard]] Epoch::Clock operator[](api::ThreadID thread_id) const;

		/**
		 * @brief Access/Modify component: VC(t). Grows the clock, so `thread_id` must be a good
		 * ID.
		 */
		[[nodiscard]] Epoch::Clock& operator[](api::ThreadID thread_id);

		/**
		 * @brief Pointwise maximum (Join): VC_1 |_| VC_2 = \lambda t. max(VC_1(t), VC_2(t)).
		 */
		VectorClock& operator|=(const VectorClock& other);

		/**
		 * @brief Epoch join: VC |_| c@t = VC[t := max(VC(t), c)]. Joining the initial epoch
		 * (null@bad) is a no-op.
		 */
		VectorClock& operator|=(const Epoch& other);

		/**
		 * @brief Partial order (Happens-before): VC_1 <= VC_2 <=> \forall t. VC_1(t) <= VC_2(t).
		 */
		[[nodiscard]] bool operator<=(const VectorClock& other) const;

		/**
		 * @brief Equality: VC_1 = VC_2 <=> \forall t. VC_1(t) = VC_2(t).
		 */
		[[nodiscard]] bool operator==(const VectorClock& other) const;

		/**
		 * @brief Increment clock for thread t: VC(t) += 1.
		 */
		void increment(api::ThreadID thread_id);

		/**
		 * @brief Number of tracked threads (size of internal clock array).
		 */
		[[nodiscard]] usize size() const { return clocks.size(); }
	};
}
