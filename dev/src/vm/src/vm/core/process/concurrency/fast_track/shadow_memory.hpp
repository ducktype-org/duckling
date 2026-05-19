#pragma once

#include <vm/core/safe/memory/memory.hpp>
#include "shadow_entry.hpp"
#include "shadow_pointer.hpp"

namespace vm {

	/**
	 * @brief Shadow memory implementation using IMemory template.
	 * Maps 1:1 with application memory bytes, but stores ShadowEntry instead.
	 */
	using ShadowMemory = IMemory<ShadowEntry>;

	/**
	 * @brief Shadow memory block.
	 */
	using ShadowBlock = BasicBlock<ShadowEntry>;

	/**
	 * @brief Shadow thread stack.
	 */
	using ShadowThreadStack = BasicThreadStack<ShadowEntry>;

	/**
	 * @brief Shadow global buffer pointers.
	 */
	using ShadowGlobalBufferPointers = GlobalBufferPointers<ShadowEntry>;

}
