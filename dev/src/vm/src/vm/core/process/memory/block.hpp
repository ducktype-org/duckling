#pragma once

#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/strongly_typed_id.hpp>

#include <vm/core/process/memory/allocator/block_data.hpp>

#include <mutex>

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
		 * block anymore, so if it's still deallocated=false, then it means we have a leak.
		 */
		u64 refcount = 0;

		/**
		 * @brief Pointer to the mutex.
		 * To avoid double dereference through the Memory class object.
		 */
		Ref<std::recursive_mutex> mutex_ref;

		// For future:
		// allocated at ...
		// freed at ...
		// name ...

		friend class Memory;

		// Think of it as a view on parent's bytes that has it's own type and lifetime.
		base::Map<usize, Ref<Block>> children_blocks{};  // offset to block
		MRef<Block>                  parent = nullptr;

	public:
		Block(BlockID id, BlockData data, Ref<std::recursive_mutex> mutex):
			  id(id),
			  data(data),
			  mutex_ref(mutex) {}
	};
}
