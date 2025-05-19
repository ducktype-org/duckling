#pragma once

#include "base/maps.hpp"
#include <base/ints.hpp>
#include <base/raw_view.hpp>
#include <base/strongly_typed_int.hpp>

#include <vm/core/process/memory/allocator/block_data.hpp>

#include <shared_mutex>
#include <utility>

namespace vm {

	// @TODO: change to STRONG_TYPEDEF_IT when available
	STRONG_TYPEDEF_INT_DIMENSIONAL(BlockID, u64);

	/**
	 * @brief Main block data structure.
	 *
	 * Holds all the block metadata and pointers to the real data.
	 * The blocks are managed by the `vm::Memory` class.
	 */
	class Block {
	private:
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
		 * @brief Flag whether the block is used.
		 * The block is not used, if it is inside the "free_ids" list of the Memory class.
		 * It can be reused for a new block.
		 */
		bool used = true;

		/**
		 * @brief The reference count of the block.
		 */
		u64 refcount = 0;

		/**
		 * @brief Pointer to the shared mutex.
		 * To avoid double dereference through the Memory class object.
		 */
		Ref<std::shared_mutex> shared_mutex;

		// For future:
		// allocated at ...
		// freed at ...
		// name ...

		friend class Memory;
		friend class VariantAllocator;

		// Think of it as a view on parent's bytes that has it's own type and lifetime.
        base::Map<usize, Ref<Block>> children_blocks{};  // offset to block
		MRef<Block>                 parent = nullptr;

	public:
		Block(BlockID id, BlockData data, Ref<std::shared_mutex> mutex):
			  id(id),
			  data(data),
			  shared_mutex(mutex) {}
	};
}
