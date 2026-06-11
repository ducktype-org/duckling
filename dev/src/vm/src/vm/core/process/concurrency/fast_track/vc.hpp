#pragma once

/**
 * @file vc.hpp
 * @brief Vector Clock implementation for the FastTrack algorithm.
 */

#include <base/types/ints.hpp>

#include <vm/api/data/thread_id.hpp>

#include <vector>

namespace vm {
	class Epoch;

	/**
	 * @brief Vector Clock (VC) used to track the happens-before relationship.
	 * Denoted in literature as V or VC.
	 */
	class VectorClock {
	private:
		std::vector<i32> clocks;
		u32              ref_count = 0;

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
		 * @brief Manual reference counting for Shared mode in ShadowEntry.
		 */
		void incRef() { ref_count++; }

		void decRef() {
			if (--ref_count == 0) delete this;
		}

		/**
		 * @brief Factory method for heap-allocated VCs used in Shared mode.
		 */
		static VectorClock* createShared() {
			auto* vc = new VectorClock();
			vc->incRef();
			return vc;
		}

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
		 * @brief Increment clock for thread t: VC(t) += 1.
		 */
		void increment(api::ThreadID thread_id);

		/**
		 * @brief Number of tracked threads (size of internal clock array).
		 */
		[[nodiscard]] usize size() const { return clocks.size(); }

	};
}
