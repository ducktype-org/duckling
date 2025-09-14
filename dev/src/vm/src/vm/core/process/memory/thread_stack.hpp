#pragma once

#include "frame.hpp"

#include <vector>

namespace vm {
	class ThreadStack final {
	private:
		std::vector<Frame>     frame_stack;
		std::vector<std::byte> local_stack;

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

		ThreadStack(): frame_stack(FRAMES_LENGTH), local_stack(STACK_LENGTH) {}

		auto getFrameStack() -> Ref<std::vector<Frame>> { return &frame_stack; }

		auto getLocalStack() -> Ref<std::vector<std::byte>> { return &local_stack; }
	};
}
