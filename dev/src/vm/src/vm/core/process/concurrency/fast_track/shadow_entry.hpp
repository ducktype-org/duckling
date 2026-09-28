#pragma once

#include "epoch.hpp"
#include "vc.hpp"

#include <vm/api/data/thread_id.hpp>

#include <memory>

namespace vm {
	/**
	 * @brief FastTrack state of a single shadowed memory location.
	 *
	 * `last_write` is the epoch of the last write. Reads are tracked in one of two modes: as long
	 * as every read is ordered after the previous one, `last_read_epoch` holds the last reader's
	 * epoch (exclusive mode); the first read concurrent with the previous one switches the entry
	 * to shared mode, where `last_read_vc` records the last read of every thread. A write switches
	 * the entry back to exclusive mode.
	 *
	 * Every entry owns its clock, so copying an entry copies its read set instead of sharing it
	 * with another location.
	 */
	struct ShadowEntry final {
		Epoch                        last_write;
		Epoch                        last_read_epoch;
		std::unique_ptr<VectorClock> last_read_vc;

		ShadowEntry() = default;
		ShadowEntry(const ShadowEntry& other);
		ShadowEntry& operator=(const ShadowEntry& other);
		ShadowEntry(ShadowEntry&& other) noexcept            = default;
		ShadowEntry& operator=(ShadowEntry&& other) noexcept = default;
		~ShadowEntry()                                       = default;

		/**
		 * @brief Resets the shadow entry to its initial state: not accessed yet.
		 */
		void reset() {
			last_write      = Epoch();
			last_read_epoch = Epoch();
			last_read_vc.reset();
		}

		[[nodiscard]] bool isShared() const { return last_read_vc != nullptr; }

		/**
		 * @brief FastTrack read rule, for a read by thread `tid` at `clock` with the vector clock
		 * `thread_vc`. The entry is updated before a race is reported, so its state is consistent
		 * when the exception unwinds.
		 * @throws exceptions::VMDataRaceException on a write-read race.
		 */
		void processRead(api::ThreadID tid, Epoch::Clock clock, const VectorClock& thread_vc);

		/**
		 * @brief FastTrack write rule, for a write by thread `tid` at `clock` with the vector
		 * clock `thread_vc`. The entry is updated before a race is reported, so its state is
		 * consistent when the exception unwinds.
		 * @throws exceptions::VMDataRaceException on a write-write or read-write race.
		 */
		void processWrite(api::ThreadID tid, Epoch::Clock clock, const VectorClock& thread_vc);

		/**
		 * @brief Reports the race between the `earlier` access and the `current` one.
		 * @throws exceptions::VMDataRaceException always.
		 */
		[[noreturn]] static void reportRace(const char* race_type, Epoch earlier, Epoch current);
	};

	inline void ShadowEntry::processRead(
		api::ThreadID tid, Epoch::Clock clock, const VectorClock& thread_vc
	) {
		const Epoch current(tid, clock);
		const Epoch racing_write    = last_write;
		const bool  write_read_race = !(last_write <= thread_vc);

		if (isShared()) {
			(*last_read_vc)[tid] = clock;
		} else if (last_read_epoch.tid() == tid || last_read_epoch <= thread_vc) {
			last_read_epoch = current;
		} else {
			// A read concurrent with the previous one: keep both, in shared mode.
			last_read_vc = std::make_unique<VectorClock>();
			*last_read_vc |= last_read_epoch;
			(*last_read_vc)[tid] = clock;
		}

		if (write_read_race) reportRace("Write-Read", racing_write, current);
	}

	inline void ShadowEntry::processWrite(
		api::ThreadID tid, Epoch::Clock clock, const VectorClock& thread_vc
	) {
		const Epoch current(tid, clock);
		const char* race = nullptr;
		Epoch       racing_access;

		if (!(last_write <= thread_vc)) {
			race          = "Write-Write";
			racing_access = last_write;
		} else if (isShared()) {
			const VectorClock& read_vc = *last_read_vc;
			if (!(read_vc <= thread_vc)) {
				race = "Read-Write (Shared)";
				for (usize i = 0; i < read_vc.size(); ++i) {
					const api::ThreadID reader{ i };
					if (read_vc[reader] > thread_vc[reader]) {
						racing_access = Epoch(reader, read_vc[reader]);
						break;
					}
				}
			}
		} else if (!(last_read_epoch <= thread_vc)) {
			race          = "Read-Write (Exclusive)";
			racing_access = last_read_epoch;
		}

		last_write      = current;
		last_read_epoch = current;
		last_read_vc.reset();

		if (race != nullptr) reportRace(race, racing_access, current);
	}
}
