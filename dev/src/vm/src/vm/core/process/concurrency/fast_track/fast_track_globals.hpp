#pragma once

#include <vm/core/process/concurrency/fast_track/shadow_entry.hpp>
#include <vm/core/safe/memory/memory.hpp>

#include <base/pointers/ref.hpp>

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
		IMemory<ShadowEntry>      shadow_data_memory;

		std::vector<ShadowEntry>  global_shadow_data;
		std::vector<ShadowBlock*> global_shadow_blocks;

		std::vector<ShadowBlock*> shadow_by_id;

	public:
		FastTrackGlobals()  = default;
		~FastTrackGlobals();

		FastTrackGlobals(const FastTrackGlobals&)            = delete;
		FastTrackGlobals& operator=(const FastTrackGlobals&) = delete;

		/**
		 * @brief Initialises (or incrementally updates) the global shadow buffers from the
		 * currently loaded program. Mirrors the FT block in SafeVMProcess::updateGlobalDataMemory.
		 */
		void initialize(CRef<low::ILowVMProgram> program);

		[[nodiscard]] ShadowEntry* globalShadowDataBase() {
			return global_shadow_data.data();
		}

		[[nodiscard]] Ref<ShadowBlock> getGlobalShadowBlock(u64 idx) {
			return { global_shadow_blocks.at(idx) };
		}

		[[nodiscard]] IMemory<ShadowEntry>& getShadowDataMemory() { return shadow_data_memory; }

		void registerShadow(BlockID id, ShadowBlock* sb) {
			auto idx = static_cast<usize>(id);
			if (shadow_by_id.size() <= idx)
				shadow_by_id.resize(idx + 1, nullptr);
			shadow_by_id[idx] = sb;
		}

		void clearShadow(BlockID id) {
			auto idx = static_cast<usize>(id);
			if (idx < shadow_by_id.size())
				shadow_by_id[idx] = nullptr;
		}

		[[nodiscard]] ShadowBlock* getShadow(BlockID id) const {
			auto idx = static_cast<usize>(id);
			if (idx >= shadow_by_id.size()) return nullptr;
			return shadow_by_id[idx];
		}
	};
}
