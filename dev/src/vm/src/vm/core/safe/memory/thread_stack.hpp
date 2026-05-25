#pragma once

#include "frame.hpp"

#include <vm/core/process/concurrency/fast_track/shadow_entry.hpp>
#include <vm/core/process/concurrency/fast_track/shadow_pointer.hpp>

#include <vector>

namespace vm {
	template<typename EntryT>
	class BasicThreadStack final {
	private:
		std::vector<Frame>     frame_stack;
		std::vector<EntryT>    local_stack;
		std::vector<BasicBlock<EntryT>*>    block_ref_stack;
		std::vector<ShadowBlock*>          shadow_block_ref_stack;
		std::vector<ShadowPointerBlock*>   shadow_pointer_block_ref_stack;
		std::vector<ShadowFrame>           shadow_frame_stack;

		std::vector<ShadowEntry>    shadow_data_stack;
		std::vector<ShadowPointer>  shadow_pointer_stack;

	public:
		static constexpr u64 FRAMES_LENGTH = 16'384;

		/**
		 * BITS_PER_FRAME means if frames take on average BITS_PER_FRAME bytes
		 * of stack space, then there can be at most FRAMES_LENGTH frames
		 * on the stack, but if functions on average take more than BITS_PER_FRAME bytes of space
		 * then fewer frames will be able to fit.
		 */
		static constexpr u64 BYTES_PER_FRAME = 256;

		static constexpr u64 STACK_LENGTH = FRAMES_LENGTH * BYTES_PER_FRAME;

		/**
		 * Also some heuristic limit on the number of blocks.
		 */
		static constexpr u64 BLOCKS_PER_FRAME = BYTES_PER_FRAME / 8;

		static constexpr u64 BLOCK_REF_STACK_LENGTH = FRAMES_LENGTH * BLOCKS_PER_FRAME;

		BasicThreadStack():
			  frame_stack(FRAMES_LENGTH),
			  local_stack(STACK_LENGTH),
			  block_ref_stack(BLOCK_REF_STACK_LENGTH),
			  shadow_block_ref_stack(BLOCK_REF_STACK_LENGTH),
			  shadow_pointer_block_ref_stack(BLOCK_REF_STACK_LENGTH),
			  shadow_frame_stack(FRAMES_LENGTH),
			  shadow_data_stack(STACK_LENGTH),
			  shadow_pointer_stack(STACK_LENGTH) {}

		auto getFrameStack() -> Ref<std::vector<Frame>> { return &frame_stack; }

		auto getLocalStack() -> Ref<std::vector<EntryT>> { return &local_stack; }

		auto getShadowDataStack() -> Ref<std::vector<ShadowEntry>> { return &shadow_data_stack; }

		auto getShadowPointerStack() -> Ref<std::vector<ShadowPointer>> { return &shadow_pointer_stack; }

		auto getBlockRefStack() -> Ref<std::vector<BasicBlock<EntryT>*>> { return &block_ref_stack; }

		auto getShadowBlockRefStack() -> Ref<std::vector<ShadowBlock*>> { return &shadow_block_ref_stack; }

		auto getShadowPointerBlockRefStack() -> Ref<std::vector<ShadowPointerBlock*>> { return &shadow_pointer_block_ref_stack; }

		auto getShadowFrameStack() -> Ref<std::vector<ShadowFrame>> { return &shadow_frame_stack; }
	};

	using ThreadStack = BasicThreadStack<std::byte>;
}
