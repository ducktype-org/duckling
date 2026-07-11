#pragma once

#include <base/pointers/ref.hpp>

#include <vm/core/process/concurrency/fast_track/epoch.hpp>
#include <vm/core/process/concurrency/fast_track/shadow_entry.hpp>
#include <vm/core/safe/memory/memory.hpp>

#include <vector>

namespace vm {
	namespace low {
		class ILowVMProgram;
	}

	/**
	 * @brief Process-level Fast Track state: global shadow buffers and shadow memory allocators.
	 *
	 * Extracted from SafeVMProcess so that FastTrackSafeVMProcess can own it while basic
	 * SafeVMProcess carries no Fast Track overhead.
	 */
	class FastTrackGlobals {
		GenericMemory<ShadowEntry> shadow_data_memory;

		std::vector<ShadowEntry>  global_shadow_data;
		std::vector<ShadowBlock*> global_shadow_blocks;

		std::vector<ShadowBlock*> shadow_by_id;

		// Stores the freeing epoch for heap blocks whose shadow has been torn down but whose
		// block ID may still be held by a concurrent thread (e.g. child running after join).
		// Sentinel: Epoch() (tid == ThreadID::bad()) means "no zombie".
		std::vector<Epoch> zombie_by_id;

	public:
		FastTrackGlobals() = default;
		~FastTrackGlobals();

		FastTrackGlobals(const FastTrackGlobals&)            = delete;
		FastTrackGlobals& operator=(const FastTrackGlobals&) = delete;

		/**
		 * @brief Initialises (or incrementally updates) the global shadow buffers from the
		 * currently loaded program. Mirrors the FT block in SafeVMProcess::updateGlobalDataMemory.
		 */
		void initialize(CRef<low::ILowVMProgram> program);

		[[nodiscard]] ShadowEntry* globalShadowDataBase() { return global_shadow_data.data(); }

		[[nodiscard]] Ref<ShadowBlock> getGlobalShadowBlock(u64 idx) {
			return { global_shadow_blocks.at(idx) };
		}

		[[nodiscard]] GenericMemory<ShadowEntry>& getShadowDataMemory() { return shadow_data_memory; }

		void registerShadow(BlockID id, ShadowBlock* sb) {
			auto idx = static_cast<usize>(id);
			if (shadow_by_id.size() <= idx) shadow_by_id.resize(idx + 1, nullptr);
			shadow_by_id[idx] = sb;
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
