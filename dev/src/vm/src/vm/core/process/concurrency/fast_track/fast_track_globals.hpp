#pragma once

#include <vm/core/process/concurrency/fast_track/shadow_entry.hpp>
#include <vm/core/process/concurrency/fast_track/shadow_pointer.hpp>
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
		IMemory<ShadowEntry>             shadow_data_memory;
		IMemory<ShadowPointer>           shadow_pointer_memory;

		std::vector<ShadowEntry>         global_shadow_data;
		std::vector<ShadowPointer>       global_shadow_pointer;
		std::vector<ShadowBlock*>        global_shadow_blocks;
		std::vector<ShadowPointerBlock*> global_shadow_pointer_blocks;

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

		[[nodiscard]] ShadowEntry*   globalShadowDataBase() {
			return global_shadow_data.data();
		}
		[[nodiscard]] ShadowPointer* globalShadowPointerBase() {
			return global_shadow_pointer.data();
		}

		[[nodiscard]] Ref<ShadowBlock> getGlobalShadowBlock(u64 idx) {
			return { global_shadow_blocks.at(idx) };
		}
		[[nodiscard]] Ref<ShadowPointerBlock> getGlobalShadowPointerBlock(u64 idx) {
			return { global_shadow_pointer_blocks.at(idx) };
		}

		[[nodiscard]] IMemory<ShadowEntry>&   getShadowDataMemory()    { return shadow_data_memory; }
		[[nodiscard]] IMemory<ShadowPointer>& getShadowPointerMemory() { return shadow_pointer_memory; }
	};
}
