#pragma once

#include "allocator/block_data.hpp"
#include "allocator/dummy_allocator.hpp"
#include "allocator/heap_allocator.hpp"
#include "block.hpp"
#include "pointer.hpp"
#include "thread_stack.hpp"

#include <base/exceptions.hpp>
#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/raw_view.hpp>
#include <base/ref.hpp>
#include <base/stable_container.hpp>

#include <vm/core/process/exceptions.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>

#include <deque>
#include <mutex>

namespace vm {
	/**
	 * @brief A memory module for a process.
	 * All of process'es memory - thread stacks (thread local data) and global data is stored here.
	 */
	class Memory final {
	private:
		// We either have recursive_mutex or a shared_mutex.
		// https://stackoverflow.com/questions/36619715/a-shared-recursive-mutex-in-standard-c
		std::recursive_mutex mutex;
		HeapAllocator        heap_allocator;
		DummyAllocator       dummy_allocator;

		std::deque<ThreadStack> threads_frame_stacks;

		base::HashMap<GlobalDataID, base::OwningView> global_data;
		base::HashMap<GlobalDataID, Ref<Block>>       global_blocks;

		// Here we use a simple recycling mechanism for blocks to avoid unnecessary allocations.
		// After the block is destroyed and the reference count drops to zero, instead of freeing
		// the memory, we mark the block as unused and add its ID to the free_ids list. Then when we
		// need to allocate a new block, we first check if there are any free IDs available. Blocks
		// are stored in a deque, so we can have pointers to them without worrying about
		// reallocation.
		std::deque<Block>   blocks   = {};
		std::deque<BlockID> free_ids = {};

		[[nodiscard]]
		Ref<Block> createBlock(BlockData data);

		void deleteBlock(Ref<Block> block);

		void destroyReference(Ref<Block> block);

		[[nodiscard]]
		Ref<Block> getBlock(BlockID id);

		/**
		 * @brief Copies blocks from `block_src` to `block_dst`, going down the nested block
		 * hierarchy.
		 */
		void copyBlocksRecursively(Ref<Block> block_dst, Ref<Block> block_src);

		/**
		 * @brief Copies `byte_size` bytes pointed-to by `src` to `dst`.
		 * @note Frees *block_dst's nested blocks whose offsets would not fit
		   inside the new memory area.
		 */
		auto copyPointedDataAndEraseSuffix(Pointer dst, Pointer src, usize byte_size) -> void;

		/**
		 * @brief Based on block's type, performs destruction of the data.
		 * E.g. in case of a non-null pointer, decreased pointed block's reference count.
	     */
		void runDataDestructor(Ref<Block> block);

		void runDataDestructorAt() {}

	public:
		Memory() = default;

		// =================== Used by executor ===================

		auto initializeFrameStack() -> Ref<ThreadStack>;

		auto allocateHeap(TypeCRef type) -> Ref<Block>;

		/**
		 * @brief Allocates a contiguous new block of memory for n elements of `type`'s inner type.
		 * @note Assumes that type is a dynamic table type.
		 */
		auto dynTableAllocateHeapN(TypeCRef type, u64 n) -> Ref<Block>;

		/**
		 * @brief Creates a block with externally managed data life-time.
		 */
		auto allocateDummy(TypeCRef type, Ref<std::byte> stack_pointer) -> Ref<Block>;

		/**
		 * @brief Dynamically reallocates block data.
		 * @note Assumes that type is a dynamic table type and reallocates it to
		   a table of size n with elements of type equal to type's inner type.
		 */
		auto dynTableReallocateBlockDataN(Ref<Block> block, u64 n) -> void;

		void freeBlock(Ref<Block> block);

		/**
		 * @brief Attempts to insert global data associated with the given ID.
		 *
		 * @param id The unique identifier for the global data.
		 * @param type The type reference to associate with the global data.
		 * @return true if the global data was inserted successfully (i.e., it did not already
		 * exist); false otherwise.
		 */
		bool tryInsertGlobalData(GlobalDataID id, TypeCRef type);

		/**
		 * @brief Returns a view of global data by the id.
		 */
		[[nodiscard]] constexpr __attribute__((always_inline)) auto getGlobalViewUnsafe(
			GlobalDataID id
		) -> base::ModRawView {
			std::lock_guard lock(mutex);
			return global_data.atMaybe(id).expect("Id not stored!")->modView();
		}

		/**
		 * @brief Returns a reference to the block appropriate for the global data by id.
		 * @note This should be the preferred method of accessing global data, if applicable.
		 */
		[[nodiscard]] constexpr __attribute__((always_inline)) auto getGlobalData(GlobalDataID id)
			-> Ref<Block> {
			std::lock_guard lock(mutex);
			return *global_blocks.atMaybe(id).expect("Id not stored!");
		}

		// =================== Variant operations ===================

		/**
		 * @brief Gets the nested block from block at offset.
		 * @note Parent pointer also stores offset within the parent block where to take the
		 * nested view block from.
		 */
		static MRef<Block> getNestedViewBlock(Pointer parent_pointer, TypeCRef type);

		/**
		 * @brief Creates new block at position.
		 * @note Parent pointer also stores offset within the parent block where to create the new
		 * nested view block.
		 */
		void setNestedViewBlock(Pointer parent_pointer, TypeCRef type);

		// =================== Block operations ===================

		[[nodiscard]]
		static auto getBlockType(Ref<Block> block) -> TypeCRef;

		// ======================== Pointers ========================

		[[nodiscard]]
		static auto newBlockReference(Ref<Block> block, u64 offset) -> Pointer;

		// @todo panics slows down the execution of the code in executor
		// we should implement entirely different error handling (maybe exception free)
		[[nodiscard]]
		static constexpr
			__attribute__((always_inline)) auto getPointerData(Pointer pointer, u64 size_bytes)
				-> base::ModRawView {
			if (pointer.block == nullptr) throw exceptions::VMNullPointerAccessException();
			std::lock_guard lock(*pointer.block->mutex_ref);
			if (pointer.block->deallocated) throw exceptions::VMUseAfterFreeException();
			if (pointer.offset + size_bytes > pointer.block->data.view.size())
				throw exceptions::VMOutOfBlockBoundsException();
			return { pointer.block->data.view.getBegin() + pointer.offset, size_bytes };
		}

		/**
		 * @brief Copies data pointed-to by `src` to `dst`.
		 * @note Assumes that the size of `type` is known at compile time and (implicitly)
		 * that it is the type of the blocks pointed-to by `dst` and `src`.
		 */
		auto copyPointedData(Pointer dst, Pointer src, TypeCRef type) -> void;

		auto destroyBlockReference(Pointer pointer) -> void;

		auto setPointer(Pointer& dst, Pointer src) -> void;

		// ======================== Requests ========================

		[[nodiscard]]
		auto requestBlockIDs() -> std::vector<BlockID>;

		[[nodiscard]]
		auto requestBlockID(Ref<Block> block) -> BlockID;

		[[nodiscard]]
		auto requestBlockData(BlockID id) -> base::RawView;

		[[nodiscard]]
		auto requestBlockType(BlockID id) -> TypeCRef;
	};
}
