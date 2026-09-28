#pragma once

#include "shadow_entry.hpp"

#include <vm/core/safe/memory/block.hpp>
#include <vm/core/safe/memory/memory.hpp>

namespace vm {
	/**
	 * @brief The memory flavour that holds the Fast Track state: one `ShadowEntry` per shadowed
	 * object instead of one byte per byte, see `Type::getShadowSize`.
	 */
	using ShadowMemory = GenericMemory<ShadowEntry>;

	using ShadowBlock = GenericBlock<ShadowEntry>;

	using ShadowGlobalBufferPointers = GlobalBufferPointers<ShadowEntry>;

	// The members are emitted once by the explicit instantiation definition in memory.cpp.
	extern template class GenericMemory<ShadowEntry>;
}
