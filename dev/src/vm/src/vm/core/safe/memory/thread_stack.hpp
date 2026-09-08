#pragma once

#include "frame.hpp"

#include <vector>

namespace vm {
	template<typename EntryT>
	class GenericThreadStack final {
	private:
		std::vector<Frame>                 frame_stack;
		std::vector<EntryT>                local_stack;
		std::vector<GenericBlock<EntryT>*> block_ref_stack;
		std::vector<LocalSlot>             slot_stack;

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

		GenericThreadStack():
			  frame_stack(FRAMES_LENGTH),
			  local_stack(STACK_LENGTH),
			  block_ref_stack(BLOCK_REF_STACK_LENGTH),
			  slot_stack(BLOCK_REF_STACK_LENGTH) {}

		auto getFrameStack() -> Ref<std::vector<Frame>> { return &frame_stack; }

		auto getLocalStack() -> Ref<std::vector<EntryT>> { return &local_stack; }

		auto getBlockRefStack() -> Ref<std::vector<GenericBlock<EntryT>*>> {
			return &block_ref_stack;
		}

		auto getSlotStack() -> Ref<std::vector<LocalSlot>> { return &slot_stack; }
	};

	using ThreadStack = GenericThreadStack<byte>;
}
