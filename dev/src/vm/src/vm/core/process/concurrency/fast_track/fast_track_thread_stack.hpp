#pragma once

#include "shadow_entry.hpp"

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/memory/thread_stack.hpp>

#include <vector>

namespace vm {
	/**
	 * @brief Shadow counterpart of `Frame`: where the shadow entries of a function's locals live.
	 * Kept on its own stack so that the data frames stay untouched when Fast Track is off.
	 */
	struct ShadowFrame final {
		ShadowEntry* local_shadow_data_stack = nullptr;
		u32          local_shadow_data_head  = 0;
	};

	/**
	 * @brief Shadow counterpart of `ThreadStack`: the shadow entries and shadow frames of one
	 * thread.
	 */
	class FastTrackThreadStack final {
	public:
		/**
		 * @brief Shadow entries budgeted per frame, the counterpart of
		 * `ThreadStack::BYTES_PER_FRAME`. Most locals are 8-byte scalars and pointers, one entry
		 * each, hence the division; a frame of many small locals draws more on the budget, one of
		 * a few large aggregates less. Whoever pushes a frame checks against the end of the stack,
		 * so a program that outgrows the shadow stack fails with a stack overflow, like one that
		 * outgrows the data stack.
		 */
		static constexpr u64 ENTRIES_PER_FRAME = ThreadStack::BYTES_PER_FRAME / 8;

		static constexpr u64 SHADOW_STACK_LENGTH = ThreadStack::FRAMES_LENGTH * ENTRIES_PER_FRAME;

	private:
		std::vector<ShadowEntry> shadow_data_stack;
		std::vector<ShadowFrame> shadow_frame_stack;

	public:
		FastTrackThreadStack():
			  shadow_data_stack(SHADOW_STACK_LENGTH),
			  shadow_frame_stack(ThreadStack::FRAMES_LENGTH) {}

		auto getShadowDataStack() -> Ref<std::vector<ShadowEntry>> { return &shadow_data_stack; }

		auto getShadowFrameStack() -> Ref<std::vector<ShadowFrame>> { return &shadow_frame_stack; }
	};
}
