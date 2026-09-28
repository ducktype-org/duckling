#pragma once

#include "epoch.hpp"
#include "shadow_memory.hpp"

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/api/data/thread_id.hpp>
#include <vm/core/safe/memory/block.hpp>

#include <vector>

namespace vm {
	namespace low {
		class ILowVMProgram;
	}

	/**
	 * @brief Process-wide Fast Track state: the shadow memory (global shadow buffer included) and
	 * the map from data blocks to their shadow blocks. Meant to be owned by the Fast Track flavour
	 * of the process, so that a plain `SafeVMProcess` carries no Fast Track overhead.
	 *
	 * Nothing in the VM drives it yet: the process, thread and opcode wiring lands with #3559.
	 */
	class FastTrackGlobals final {
		ShadowMemory shadow_data_memory;

		/// Shadow block of every shadowed data block, indexed by `BlockID`. `nullptr` where the
		/// block has no shadow (yet).
		std::vector<ShadowBlock*> shadow_by_id;

		/// The epoch at which a heap block's shadow was torn down, indexed by `BlockID`. A thread
		/// that still holds the block's ID (e.g. a child running after the free) races with it
		/// unless it is ordered after that epoch. `Epoch()` means "not a zombie".
		std::vector<Epoch> zombie_by_id;

		/// Count of the globals that already have a shadow block, see `initializeNewGlobalBlocks`.
		usize shadowed_global_count = 0;

	public:
		/// Every global starts as written by the main thread at its first epoch, before any other
		/// thread exists.
		static constexpr Epoch GLOBAL_INITIAL_EPOCH{ api::MAIN_THREAD_ID, 1 };

		FastTrackGlobals() = default;

		FastTrackGlobals(const FastTrackGlobals&)            = delete;
		FastTrackGlobals& operator=(const FastTrackGlobals&) = delete;

		/**
		 * @brief Creates the shadow blocks of the globals of `program` that do not have one yet.
		 * The shadow counterpart of `SafeVMProcess::updateGlobalDataMemory`: incremental, and it
		 * may move the global shadow buffer.
		 * @warning Every thread has to refresh its global shadow pointers from the returned value.
		 */
		ShadowGlobalBufferPointers initializeNewGlobalBlocks(CRef<low::ILowVMProgram> program);

		/**
		 * @brief Frees the shadow of every global, the counterpart of `Memory::deinitGlobals`.
		 */
		void deinitGlobals() { shadow_data_memory.deinitGlobals(); }

		[[nodiscard]] ShadowGlobalBufferPointers getGlobalShadowMemory() {
			return shadow_data_memory.getGlobalDataMemory();
		}

		[[nodiscard]] ShadowMemory& getShadowDataMemory() { return shadow_data_memory; }

		void registerShadow(BlockID id, ShadowBlock* shadow) {
			auto idx = static_cast<usize>(id);
			if (shadow_by_id.size() <= idx) shadow_by_id.resize(idx + 1, nullptr);
			shadow_by_id[idx] = shadow;
		}

		void clearShadow(BlockID id) {
			auto idx = static_cast<usize>(id);
			if (idx < shadow_by_id.size()) shadow_by_id[idx] = nullptr;
		}

		[[nodiscard]] ShadowBlock* getShadow(BlockID id) const {
			auto idx = static_cast<usize>(id);
			if (idx >= shadow_by_id.size()) return nullptr;
			return shadow_by_id[idx];
		}

		void addZombie(BlockID id, Epoch epoch) {
			auto idx = static_cast<usize>(id);
			if (zombie_by_id.size() <= idx) zombie_by_id.resize(idx + 1, Epoch{});
			zombie_by_id[idx] = epoch;
		}

		void removeZombie(BlockID id) {
			auto idx = static_cast<usize>(id);
			if (idx < zombie_by_id.size()) zombie_by_id[idx] = Epoch{};
		}

		[[nodiscard]] Epoch getZombie(BlockID id) const {
			auto idx = static_cast<usize>(id);
			if (idx >= zombie_by_id.size()) return Epoch{};
			return zombie_by_id[idx];
		}
	};
}
