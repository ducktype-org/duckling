#pragma once

#include <vm/core/process/concurrency/fast_track/shadow_entry.hpp>
#include <vm/core/process/concurrency/fast_track/shadow_pointer.hpp>
#include <vm/core/safe/memory/frame.hpp>
#include <vm/core/safe/memory/thread_stack.hpp>

#include <vector>

namespace vm {
	class FastTrackThreadStack final {
		std::vector<ShadowEntry>         shadow_data_stack;
		std::vector<ShadowPointer>       shadow_pointer_stack;
		std::vector<ShadowFrame>         shadow_frame_stack;
		std::vector<ShadowBlock*>        shadow_block_ref_stack;
		std::vector<ShadowPointerBlock*> shadow_pointer_block_ref_stack;

	public:
		FastTrackThreadStack():
			  shadow_data_stack(ThreadStack::STACK_LENGTH),
			  shadow_pointer_stack(ThreadStack::STACK_LENGTH),
			  shadow_frame_stack(ThreadStack::FRAMES_LENGTH),
			  shadow_block_ref_stack(ThreadStack::BLOCK_REF_STACK_LENGTH),
			  shadow_pointer_block_ref_stack(ThreadStack::BLOCK_REF_STACK_LENGTH) {}

		auto getShadowDataStack() -> Ref<std::vector<ShadowEntry>> { return &shadow_data_stack; }
		auto getShadowPointerStack() -> Ref<std::vector<ShadowPointer>> {
			return &shadow_pointer_stack;
		}
		auto getShadowFrameStack() -> Ref<std::vector<ShadowFrame>> { return &shadow_frame_stack; }
		auto getShadowBlockRefStack() -> Ref<std::vector<ShadowBlock*>> {
			return &shadow_block_ref_stack;
		}
		auto getShadowPointerBlockRefStack() -> Ref<std::vector<ShadowPointerBlock*>> {
			return &shadow_pointer_block_ref_stack;
		}
	};
}
