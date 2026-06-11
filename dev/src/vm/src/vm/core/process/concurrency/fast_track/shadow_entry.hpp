#pragma once

#include "epoch.hpp"
#include "vc.hpp"

#include <vm/api/data/api_error.hpp>
#include <vm/core/safe/exceptions.hpp>

#include <iostream>
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
		VectorClock* last_read_vc = nullptr;  // Manual ref-counting in Phase 3

		ShadowEntry(): last_write(), last_read_epoch() {}

		/**
		 * @brief Resets the shadow entry to its initial state.
		 */
		void reset() {
			last_write      = Epoch();
			last_read_epoch = Epoch();
			if (last_read_vc) {
				last_read_vc->decRef();
				last_read_vc = nullptr;
			}
		}

		~ShadowEntry() {
			if (last_read_vc) last_read_vc->decRef();
		}

		// Support copy for IMemory/std::fill/std::copy
		ShadowEntry(const ShadowEntry& other):
			  last_write(other.last_write),
			  last_read_epoch(other.last_read_epoch),
			  last_read_vc(other.last_read_vc) {
			if (last_read_vc) last_read_vc->incRef();
		}

		ShadowEntry& operator=(const ShadowEntry& other) {
			if (this != &other) {
				if (last_read_vc) last_read_vc->decRef();
				last_write      = other.last_write;
				last_read_epoch = other.last_read_epoch;
				last_read_vc    = other.last_read_vc;
				if (last_read_vc) last_read_vc->incRef();
			}
			return *this;
		}

		// Allow move
		ShadowEntry(ShadowEntry&& other) noexcept:
			  last_write(other.last_write),
			  last_read_epoch(other.last_read_epoch),
			  last_read_vc(other.last_read_vc) {
			other.last_read_vc = nullptr;
		}

		ShadowEntry& operator=(ShadowEntry&& other) noexcept {
			if (this != &other) {
				if (last_read_vc) last_read_vc->decRef();
				last_write         = other.last_write;
				last_read_epoch    = other.last_read_epoch;
				last_read_vc       = other.last_read_vc;
				other.last_read_vc = nullptr;
			}
			return *this;
		}

		friend std::ostream& operator<<(std::ostream& ss, const ShadowEntry& e) {
			ss << "write: " << e.last_write.clock() << "@" << e.last_write.tid() << '\n';
			ss << "read : " << e.last_read_epoch.clock() << "@" << e.last_read_epoch.tid();
			return ss;
		}

		[[nodiscard]] bool isShared() const { return last_read_vc != nullptr; }

		/**
		 * @brief FastTrack Read rule.
		 */
		void processRead(api::ThreadID tid, i32 clock, const VectorClock& thread_vc) {
			// WR Race: last_write <= thread_vc(tid_w)
			if (!(last_write <= thread_vc)) reportRace("Write-Read", last_write, Epoch(tid, clock));

			if (isShared()) {
				// Shared Mode: update VC(tid)
				(*last_read_vc)[tid] = clock;
			} else {
				// Exclusive Mode
				if (last_read_epoch.tid() == tid) {
					// Same thread: just update clock (optimization)
					last_read_epoch = Epoch(tid, clock);
				} else if (last_read_epoch <= thread_vc) {
					// Ordered read: update to new exclusive reader
					last_read_epoch = Epoch(tid, clock);
				} else {
					// Concurrent read: transition to Shared Mode
					last_read_vc = VectorClock::createShared();
					*last_read_vc |= last_read_epoch;
					(*last_read_vc)[tid] = clock;
				}
			}
		}

		/**
		 * @brief FastTrack Write rule.
		 */
		void processWrite(api::ThreadID tid, i32 clock, const VectorClock& thread_vc) {
			// WW Race: last_write <= thread_vc
			if (!(last_write <= thread_vc))
				reportRace("Write-Write", last_write, Epoch(tid, clock));

			// RW Race
			if (isShared()) {
				if (!(*last_read_vc <= thread_vc)) {
					Epoch conflicting_reader;
					for (usize i = 0; i < last_read_vc->size(); ++i) {
						auto t = api::ThreadID(static_cast<u64>(i));
						if ((*last_read_vc)[t] > thread_vc[t]) {
							conflicting_reader = Epoch(t, (*last_read_vc)[t]);
							break;
						}
					}
					reportRace("Read-Write (Shared)", conflicting_reader, Epoch(tid, clock));
				}
				last_read_vc->decRef();
				last_read_vc = nullptr;
			} else {
				if (!(last_read_epoch <= thread_vc))
					reportRace("Read-Write (Exclusive)", last_read_epoch, Epoch(tid, clock));
			}

			last_write      = Epoch(tid, clock);
			last_read_epoch = last_write;  // FastTrack optimization/invariant
		}

	public:
		[[noreturn]] static void reportRace(const char* race_type, Epoch epoch_a, Epoch epoch_b) {
			std::string detail = race_type;
			detail += " — t";
			detail += std::to_string(epoch_a.tid().asInt());
			detail += "@";
			detail += std::to_string(epoch_a.clock());
			detail += " vs t";
			detail += std::to_string(epoch_b.tid().asInt());
			detail += "@";
			detail += std::to_string(epoch_b.clock());
			throw exceptions::VMDataRaceException(std::move(detail));
		}
	};

}
