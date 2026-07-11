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
		std::vector<ShadowEntry> shadow_data_stack;
		std::vector<ShadowFrame> shadow_frame_stack;

	public:
		FastTrackThreadStack():
			  shadow_data_stack(ThreadStack::STACK_LENGTH),
			  shadow_frame_stack(ThreadStack::FRAMES_LENGTH) {}

		auto getShadowDataStack() -> Ref<std::vector<ShadowEntry>> { return &shadow_data_stack; }

		auto getShadowFrameStack() -> Ref<std::vector<ShadowFrame>> { return &shadow_frame_stack; }
	};
}
