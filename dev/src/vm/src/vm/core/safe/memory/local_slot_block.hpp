// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
	inline auto createLocalSlotBlock(LocalSlot& slot, Memory& memory) -> Ref<Block> {
		auto block = memory.adoptDummy(slot.type, slot.data);
		// So that nobody can delete our block.
		Memory::increaseBlockRefcount(block);

		slot.block = block.get();
		return block;
	}

	inline auto createLocalSlotBlock(Frame& frame, Memory& memory, u64 slot_index) -> Ref<Block> {
		return createLocalSlotBlock(frame.local_slot_stack_base[slot_index], memory);
	}

	/**
	 * @brief Releases what a local variable owns, without popping its slot. A variable with a
	 * block frees the block; one that never needed a block has the destructors of its data run
	 * directly, which is what freeing its block would have done.
	 */
	inline void releaseLocalSlot(const LocalSlot& slot, Memory& memory) {
		if (Block* block = slot.block) {
			memory.freeBlockData(block);
			memory.decreaseBlockRefcount(block);
			return;
		}

		memory.runDataDestructors({ slot.data, slot.type->getSize().asInt() }, slot.type);
	}
}
