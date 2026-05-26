#pragma once

#include <base/collections/maps.hpp>
#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/ints.hpp>

#include <vm/core/process/concurrency/fast_track/shadow_pointer.hpp>
#include <vm/core/process/concurrency/fast_track/shadow_entry.hpp>
#include <vm/core/safe/memory/allocator/block_data.hpp>

namespace vm {

	STRONG_TYPEDEF_ID_DIRECT_CREATION(BlockID);

	template<typename EntryT, typename BlockT>
	class IMemory;

	/**
	 * @brief Main block data structure.
	 *
	 * Holds all the block metadata and pointers to the real data.
	 * The blocks are managed by the `vm::Memory` class.
	 */
	template<typename EntryT>
	class BasicBlock {
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
		friend class IMemory;

		// Think of it as a view on parent's bytes that has it's own type and lifetime.
		base::Map<usize, Ref<BasicBlock<EntryT>>> children_blocks{};  // offset to block
		MRef<BasicBlock<EntryT>>                  parent = nullptr;

	public:
		BasicBlock(BlockID id, BlockData<EntryT> data): id(id), data(data) {}

		[[nodiscard]] EntryT* getData() { return data.view.getBegin(); }
		[[nodiscard]] const EntryT* getData() const { return data.view.getBegin(); }
		[[nodiscard]] bool isDeallocated() const { return deallocated; }
	};

	using Block = BasicBlock<std::byte>;
    using ShadowBlock = BasicBlock<ShadowEntry>;
    using ShadowPointerBlock = BasicBlock<ShadowPointer>;
	using BlockGeneric = Block;

	inline ShadowEntry* ShadowPointer::data_base() const { return shadow_block ? shadow_block->getData() : nullptr; }
	inline ShadowPointer* ShadowPointer::pointer_base() const { return shadow_pointer_block ? shadow_pointer_block->getData() : nullptr; }
}

ID_STD_HASH(vm::BlockID);
