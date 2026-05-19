#pragma once

#include "epoch.hpp"
#include "vc.hpp"

#include <memory>
#include <variant>

namespace vm {

	/**
	 * @brief Shadow state for a single memory location (byte).
	 * Follows the FastTrack algorithm state:
	 * - last_write: The epoch of the last write.
	 * - last_read: Either an Epoch (Exclusive mode) or a pointer to a Vector Clock (Shared
	 * mode).
	 */
	struct ShadowEntry {
		Epoch        last_write;
		Epoch        last_read_epoch;
		VectorClock* last_read_vc = nullptr; // Manual ref-counting in Phase 3

		ShadowEntry(): last_write(), last_read_epoch(), last_read_vc(nullptr) {}

		/**
		 * @brief Resets the shadow entry to its initial state.
		 */
		void reset() {
			last_write      = Epoch();
			last_read_epoch = Epoch();
			last_read_vc    = nullptr;
		}

		bool isShared() const { return last_read_vc != nullptr; }
	};

}
