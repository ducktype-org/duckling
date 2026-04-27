#pragma once

/**
 * @file vc.hpp
 * @brief Vector Clock implementation for the FastTrack algorithm.
 */

#include <vector>
#include <base/types/ints.hpp>
#include <vm/api/data/thread_id.hpp>

namespace vm {
	class Epoch;

	/**
	 * @brief Vector Clock (VC) used to track the happens-before relationship.
	 * Denoted in literature as V or VC.
	 */
	class VectorClock {
	private:
		std::vector<i32> clocks;

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
		 * @brief Read component: VC(t).
		 */
		i32 operator[](api::ThreadID thread_id) const;

		/**
		 * @brief Access/Modify component: VC(t).
		 */
		i32& operator[](api::ThreadID thread_id);

		/**
		 * @brief Pointwise maximum (Join): VC_1 |_| VC_2 = \lambda t. max(VC_1(t), VC_2(t)).
		 */
		VectorClock& operator|=(const VectorClock& other);

		/**
		 * @brief Epoch join: VC |_| c@t = VC[t := max(VC(t), c)].
		 */
		VectorClock& operator|=(const Epoch& other);

		/**
		 * @brief Partial order (Happens-before): VC_1 <= VC_2 <=> \forall t. VC_1(t) <= VC_2(t).
		 */
		bool operator<=(const VectorClock& other) const;

		/**
		 * @brief Equality: VC_1 = VC_2 <=> \forall t. VC_1(t) = VC_2(t).
		 */
		bool operator==(const VectorClock& other) const;

		/**
		 * @brief Strict partial order: VC_1 < VC_2 <=> VC_1 <= VC_2 \land VC_1 != VC_2.
		 */
		bool operator<(const VectorClock& other) const;

		/**
		 * @brief Inequality: VC_1 != VC_2.
		 */
		bool operator!=(const VectorClock& other) const;
	};
}
