#pragma once

#include <vm/core/process/concurrency/fast_track/shadow_entry.hpp>
#include <vm/core/safe/memory/frame.hpp>
#include <vm/core/safe/memory/thread_stack.hpp>

#include <vector>

namespace vm {
	class FastTrackThreadStack final {
		std::vector<ShadowEntry> shadow_data_stack;
		std::vector<ShadowFrame> shadow_frame_stack;

	public:
		FastTrackThreadStack():
			  shadow_data_stack(ThreadStack::STACK_LENGTH),
			  shadow_frame_stack(ThreadStack::FRAMES_LENGTH) {}

		auto getShadowDataStack()  -> Ref<std::vector<ShadowEntry>> { return &shadow_data_stack; }
		auto getShadowFrameStack() -> Ref<std::vector<ShadowFrame>> { return &shadow_frame_stack; }
	};
}
