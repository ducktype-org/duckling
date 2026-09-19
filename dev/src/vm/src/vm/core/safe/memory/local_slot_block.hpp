#pragma once

#include "frame.hpp"
#include "memory.hpp"

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

namespace vm {
	/**
	 * @brief Creates the block of a local variable that was initialized without one, out of what
	 * its slot recorded. Both the executor and the debug adapter go through here.
	 *
	 * A free function rather than a `Memory` member: it needs nothing but `Memory`'s public API,
	 * and `Memory` itself has no business knowing about frames.
	 */
	inline auto createLocalSlotBlock(const Frame& frame, Memory& memory, u64 slot_index)
		-> Ref<Block> {
		LocalSlot& slot = frame.local_slot_stack_base[slot_index];

		auto block = memory.adoptDummy(slot.type, slot.data);
		// So that nobody can delete our block.
		Memory::increaseBlockRefcount(block);

		slot.block = block.get();
		return block;
	}
}
