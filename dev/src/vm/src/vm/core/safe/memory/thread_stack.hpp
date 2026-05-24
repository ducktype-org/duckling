#pragma once

#include "frame.hpp"

#include <vm/core/safe/type_metadata/type.hpp>

#include <cstdlib>
#include <memory>
#include <vector>

namespace vm {
	class ThreadStack final {
	private:
		std::vector<Frame> frame_stack;
		std::unique_ptr<std::byte>
			local_stack;  // Memory is allocated manually, beacause it must be alligned.
		std::vector<Block*> block_ref_stack;

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

		ThreadStack():
			  frame_stack(FRAMES_LENGTH),
			  local_stack(reinterpret_cast<std::byte*>(
				  std::aligned_alloc(Type::MAX_ALIGNMENT, STACK_LENGTH)
			  )),
			  block_ref_stack(BLOCK_REF_STACK_LENGTH) {}

		auto getFrameStack() -> Ref<std::vector<Frame>> { return &frame_stack; }

		auto getLocalStackBegin() -> std::byte* { return local_stack.get(); }

		auto getLocalStackEnd() -> std::byte* { return local_stack.get() + STACK_LENGTH; }

		auto getBlockRefStack() -> Ref<std::vector<Block*>> { return &block_ref_stack; }
	};
}
