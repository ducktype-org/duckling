#pragma once

#include <base/collections/maps.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/memory/allocator/block_data.hpp>

namespace vm {

	STRONG_TYPEDEF_ID_DIRECT_CREATION(BlockID);

	template<typename EntryT, typename BlockT>
	class GenericMemory;

	/**
	 * @brief Main block data structure.
	 *
	 * Holds all the block metadata and pointers to the real data.
	 * The blocks are managed by the `vm::Memory` class.
	 */
	template<typename EntryT>
	class GenericBlock {
		/**
		 * @brief The unique identifier for the block.
		 */
		BlockID id;

		/**
		 * @brief The data of the block.
		 */
		BlockData<EntryT> data;

		/**
		 * @brief Flag whether the block has been deallocated.
		 */
		bool deallocated = false;

		/**
		 * @brief Whether manual freeing of a block is allowed.
		 * A block is freeable if it is the owning block of heap-allocated
		 * memory, other blocks are not freeable.
		 */
		bool freeable = false;

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

		template<typename E, typename B>
		friend class GenericMemory;

		// Think of it as a view on parent's bytes that has it's own type and lifetime.
		base::Map<usize, Ref<GenericBlock<EntryT>>> children_blocks{};  // offset to block
		MRef<GenericBlock<EntryT>>                  parent = nullptr;

	public:
		GenericBlock(BlockID id, BlockData<EntryT> data): id(id), data(data) {}
	};

	using Block = GenericBlock<byte>;
}

ID_STD_HASH(vm::BlockID);
