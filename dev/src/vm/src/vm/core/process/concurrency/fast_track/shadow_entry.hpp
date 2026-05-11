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
	 * - last_read: Either an Epoch (Exclusive mode) or a shared pointer to a Vector Clock (Shared
	 * mode).
	 */
	struct ShadowEntry {
		Epoch                                             last_write;
		std::variant<Epoch, std::shared_ptr<VectorClock>> last_read;

		ShadowEntry(): last_write(), last_read(Epoch()) {}

		/**
		 * @brief Resets the shadow entry to its initial state.
		 */
		void reset() {
			last_write = Epoch();
			last_read  = Epoch();
		}
	};

}
