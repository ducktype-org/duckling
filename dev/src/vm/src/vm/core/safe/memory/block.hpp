#pragma once

#include <base/collections/maps.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/memory/allocator/block_data.hpp>

namespace vm {

	STRONG_TYPEDEF_ID_DIRECT_CREATION(BlockID);

	/**
	 * @brief Main block data structure.
	 *
	 * Holds all the block metadata and pointers to the real data.
	 * The blocks are managed by the `vm::Memory` class.
	 */
	class Block {
		/**
		 * @brief The unique identifier for the block.
		 */
		BlockID id;

		/**
		 * @brief The data of the block.
		 */
		BlockData data;

		/**
		 * @brief Flag whether the block has been deallocated.
		 */
		bool deallocated = false;

		/**
		 * @brief The reference count of the block.
		 * If anybody is looking at a block (function stack, pointer, parent block, etc.), then
		 * refcount should stay positive. If refcount is dropped to 0, then nobody needs the
		 * block anymore. If at that point the block is still marked as deallocated=false, it means
		 * we have a memory leak.
		 */
		u64 refcount = 0;

		// For future:
		// allocated at ...
		// freed at ...
		// name ...

		friend class Memory;

		// Think of it as a view on parent's bytes that has it's own type and lifetime.
		base::Map<usize, Ref<Block>> children_blocks{};  // offset to block
		MRef<Block>                  parent = nullptr;

	public:
		Block(BlockID id, BlockData data): id(id), data(data) {}

		Block(u64 id, BlockData data): Block(BlockID(id), data) {}
	};
}

ID_STD_HASH(vm::BlockID);
