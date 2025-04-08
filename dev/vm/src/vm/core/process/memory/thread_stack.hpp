#pragma once

#include "frame.hpp"

#include <vector>

namespace vm {
	class ThreadStack {
	private:
		static constexpr u64 FRAMES_LENGTH = 16'384;
		static constexpr u64 STACK_LENGTH  = FRAMES_LENGTH * 256;

		std::vector<Frame>     frame_stack;
		std::vector<std::byte> local_stack;

	public:
		ThreadStack(): frame_stack(FRAMES_LENGTH), local_stack(STACK_LENGTH) {}

		auto getFrameStack() -> Ref<std::vector<Frame>> { return &frame_stack; }

		auto getLocalStack() -> Ref<std::vector<std::byte>> { return &local_stack; }
	};
}
