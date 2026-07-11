#include "fast_track_globals.hpp"

#include <vm/core/safe/low_program/low_program.hpp>

#include <ranges>
#include <vector>

namespace vm {
	ShadowGlobalBufferPointers FastTrackGlobals::initializeNewGlobalBlocks(
		CRef<low::ILowVMProgram> program
	) {
		using namespace std::ranges;
		const auto  config  = program->getGlobalBufferConfig();
		const auto& globals = program->getGlobals();

		auto pointers
			= shadow_data_memory.initializeNewGlobalBlocks(ShadowMemory::GlobalBlocksConfig{
				.global_data_offsets
				= globals | views::transform(&low::LowGlobalData::global_shadow_data_offset)
		        | to<std::vector>(),
				.global_blocks_idxs = globals
		                            | views::transform(&low::LowGlobalData::global_block_idx)
		                            | to<std::vector>(),
				.global_types
				= globals | views::transform(&low::LowGlobalData::type) | to<std::vector>(),
				.total_global_data_size = config.shadow_buffer_size,
				.global_count           = config.global_count,
			});

		for (const low::LowGlobalData& global: globals) {
			if (global.global_block_idx < shadowed_global_count) continue;
			ShadowEntry* entries = pointers.data_buffer_base + global.global_shadow_data_offset;
			for (ShadowSize i = 0; i < global.type->getShadowSize(); ++i) {
				entries[i].last_write      = GLOBAL_INITIAL_EPOCH;
				entries[i].last_read_epoch = GLOBAL_INITIAL_EPOCH;
			}
		}
		shadowed_global_count = config.global_count;

		return pointers;
	}
}
