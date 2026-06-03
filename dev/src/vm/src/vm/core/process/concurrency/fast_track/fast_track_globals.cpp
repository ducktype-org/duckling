#include "fast_track_globals.hpp"

#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/process/concurrency/fast_track/epoch.hpp>

#include <algorithm>

namespace vm {

	FastTrackGlobals::~FastTrackGlobals() {
		for (auto* block : global_shadow_blocks) {
			if (block) {
				shadow_data_memory.freeBlockData(Ref(block));
				shadow_data_memory.decreaseBlockRefcount(Ref(block));
			}
		}
	}

	void FastTrackGlobals::initialize(CRef<low::ILowVMProgram> program) {
		auto global_buffer_config = program->getGlobalBufferConfig();

		global_shadow_data.resize(
			std::max<size_t>(global_buffer_config.global_shadow_buffer_size, 1)
		);

		global_shadow_blocks.resize(global_buffer_config.global_count, nullptr);

		for (const auto& [global, id, name] : program->getGlobals().allData()) {
			auto block_idx = global->global_block_idx;

			if (global_shadow_blocks[block_idx] == nullptr) {
				auto block = shadow_data_memory.allocateDummy(
					global->type,
					global_shadow_data.data() + global->global_shadow_data_offset
				);
				shadow_data_memory.increaseBlockRefcount(block);
				global_shadow_blocks[block_idx] = block.get();
			} else {
				shadow_data_memory.updateBlockDataView(
					Ref(global_shadow_blocks[block_idx]),
					{ global_shadow_data.data() + global->global_shadow_data_offset,
					  global->type->getShadowSize() }
				);
			}

			auto* start = global_shadow_data.data() + global->global_shadow_data_offset;
			for (u32 i = 0; i < global->type->getShadowSize(); ++i) {
				start[i].last_write      = Epoch(api::ThreadID{ 0 }, 1);
				start[i].last_read_epoch = Epoch(api::ThreadID{ 0 }, 1);
			}
		}
	}
}
