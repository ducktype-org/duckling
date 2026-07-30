#pragma once

#include "allocator/block_data.hpp"
#include "allocator/dummy_allocator.hpp"
#include "allocator/heap_allocator.hpp"
#include "block.hpp"
#include "pointer.hpp"
#include "thread_stack.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/object_pool.hpp>
#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/bytecode/constant_value_fd.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>

namespace vm {
	/**
	 * @brief Helper structure that holds pointers to the global data buffer and global blocks buffer.
	 */
	struct GlobalBufferPointers {
		std::byte* data_buffer_base;    /// Base pointer to the global data buffer.
		Block**    blocks_buffer_base;  /// Base pointer to the global blocks buffer.
	};

	/**
	 * @brief A memory module for a process.
	 * @note Memory is single threaded!
	 * All of process'es memory - thread stacks (thread local data) and global data is stored here.
	 */
	class Memory final {
	private:
		HeapAllocator  heap_allocator;
		DummyAllocator dummy_allocator;

		std::deque<ThreadStack> threads_frame_stacks;

		/// Buffer for the global data
		std::vector<std::byte> global_data_buffer{};

		/// Buffer for the global blocks
		std::vector<Block*> global_data_blocks{};

		/// If the global has been initialized (constructor has been called), then it is in this
		/// set. Otherwise, it is not.
		std::unordered_set<BlockID> initialized_globals{};

		base::StableObjectPool<Block, BlockID, true, true> blocks_pool;

		[[nodiscard]]
		Ref<Block> createBlock(BlockData data);

		/**
		 * @brief Erases the block object from the memory.
		 */
		void deleteBlock(Ref<Block> block);

		[[nodiscard]]
		Ref<Block> getBlock(BlockID id);

		/**
		 * @brief Copies blocks from `block_src` to `block_dst`, going down the nested block
		 * hierarchy.
		 */
		void copyBlocksRecursively(Ref<Block> block_dst, Ref<Block> block_src);

		/**
		 * @brief Copies blocks from `block_src` to `block_dst`, going down the nested block
		 * hierarchy.
		 * @note Moving here means no data copy-constructors are called.
		 */
		void moveBlocksRecursively(Ref<Block> block_dst, Ref<Block> block_src);

		/**
		 * @brief Moves `byte_size` bytes pointed-to by `src` to `dst`.
		 * Moving here means data copy-constructors of the moved object are not invoked.
		 * The objects that are in the "suffix" are destructed.
		 * @note Frees *block_dst's nested blocks whose offsets would not fit
		 * inside the new memory area.
		 * @note These blocks must be of a dynamic table type.
		 */
		void moveBlockDataAndEraseSuffix(Ref<Block> dst, Ref<Block> src, usize byte_count);


		/**
		 * @brief Replaces the block data memory view with the new one,
		 * taking care of the children blocks.
		 *
		 * @param block The block to update the data view for.
		 * @param new_view The new view to set for the block.
		 */
		void updateBlockDataView(Ref<Block> block, base::ModRawView new_view);

		/**
		 * @brief Executes destructors on individual objects that are in the block.
		 * @param block The block to source the data from.
		 */
		void runDataDestructors(Ref<Block> block);

		/**
		 * @brief Executes destructors on a range of objects, that lay next to each other.
		 */
		void runDataDestructors(base::ModRawView data, TypeCRef type);

		/**
		 * @brief Executes copy constructors on individual objects that are in the block.
		 * @param block The block to source the data from.
		 */
		void runDataCopyConstructors(Ref<Block> block);

		/**
		 * @brief Executes copy constructors on a range of objects, that lay next to each other.
		 */
		void runDataCopyConstructors(base::ModRawView data, TypeCRef type);

		/**
		 * @brief Iterates over each object in the block and calls the callback on it.
		 * @note The callback should not change the layout of the objects in the block.
		 * @param callback A function that will be called on each object.
		 */
		void iterateOverDataAndExecute(
			Ref<Block> block, void (Memory::*callback)(base::ModRawView data, TypeCRef type)
		);

		/**
		 * @brief Iterates over each object in the `data` and calls the callback on it.
		 * @note The callback should not change the layout of the objects in the block.
		 * @note In opposition to `runObjectDestructor` and `runObjectCopyConstructor`, here `data`
		 * can represent multiple objects.
		 * @param callback A function that will be called on each object.
		 */
		void iterateOverDataAndExecute(
			base::ModRawView data,
			TypeCRef         type,
			void (Memory::*callback)(base::ModRawView data, TypeCRef type)
		);

		/**
		 * @brief Based on data's type, performs destruction of the data.
		 * E.g. in case of a non-null pointer, decreases pointed block's reference count.
		 * @note `data` has to represent a single object, not multiple objects - e.g. it can't be a
		 * range of objects from a table, but it can be a single object from a table, or from
		 * somewhere else.
		 */
		void runObjectDestructor(base::ModRawView data, TypeCRef type);

		/**
		 * @brief Based on data's type, performs copy-constructor of the data.
		 * E.g. in case of a non-null pointer, increases pointed block's reference count.
		 * @note `data` has to represent a single object, not multiple objects - e.g. it can't be a
		 * range of objects from a table, but it can be a single object from a table, or from
		 * somewhere else.
		 */
		void runObjectCopyConstructor(base::ModRawView data, TypeCRef type);

	public:
		Memory() = default;

		// =================== Used by the process ===================

		/**
		 * @brief Validates the memory state.
		 * It can be thought of as a check that is executed after program's exit
		 * to determine the correctness of memory usage
		 * (and potentially bugs inside the VM itself as well).
		 * For the memory to be valid, all the blocks' referenceCount needs to be 0. This means
		 * no leaks, etc.
		 * @return True if memory was used correctly, false otherwise.
		 */
		bool validateMemoryState() const;

		/**
		 * @brief Frees all the global data
		 */
		void deinitGlobals();

		struct GlobalBlocksConfig {
			std::vector<usize>    global_data_offsets;
			std::vector<usize>    global_blocks_idxs;
			std::vector<TypeCRef> global_types;
			Bytes                 total_global_data_size;
			usize                 global_count;
		};

		/**
		 * @brief Allocates the memory for the global variables based on the provided configuration
		 * and creates the blocks for them. Works in the incremental setting, so only the new global
		 * variables are created, and the existing ones are left unchanged. The VMProcess can call
		 * this function when new global variables are added to the program.
		 *
		 * Does not support shrinking of the global buffer or decreasing the global count,
		 * meaning it can only be used to add new global variables.
		 *
		 * @warning It may invalidate the pointers to the global buffer, after calling this function
		 * always update the obtained pointers.
		 *
		 * @param global_blocks The configuration for the global blocks to initialize.
		 * @return The new pointers to the global data buffer and global blocks buffer.
		 */
		GlobalBufferPointers initializeNewGlobalBlocks(const GlobalBlocksConfig& global_blocks);

		// =================== Used by executor ===================

		auto initializeFrameStack() -> Ref<ThreadStack>;

		/**
		 * @brief Get the pointers to the global data buffer and global blocks buffer, that can be
		 * used by the threads.
		 */
		GlobalBufferPointers getGlobalDataMemory();

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

		/**
		 * @brief Frees block's data, but not the block structure itself.
		 * For the block to be freed, use deleteBlock.
		 */
		void freeBlockData(Ref<Block> block);

		/**
		 * @brief Checks if the global variable in the block has been initialized (constructor has
		 * been called).
		 *
		 * Should be called from a place where the runtime initialization is happening.
		 * If the global variable has no constructor this function shouldn't be called.
		 *
		 * @param global_block The block of the global variable to check.
		 * @return True if the global variable has been initialized, false otherwise.
		 */
		bool isGlobalInitialized(Ref<Block> global_block);

		/**
		 * @brief Set the global as initialized (the constructor has been called).
		 *
		 * Should be called from a place where the runtime initialization is happening.
		 * If the global variable has no constructor this function shouldn't be called.
		 *
		 * @param global_block The block of the global variable to set as initialized.
		 */
		void setGlobalInitialized(Ref<Block> global_block);


		void initializeBlockFromConstValue(Ref<Block> block, const code::ConstantValue& const_value);

		/**
		 * @brief Returns a view of block's data
		 */
		[[nodiscard]] constexpr __attribute__((always_inline)) auto getBlockViewUnsafe(
			Ref<Block> block
		) -> base::ModRawView {
			return block->data.view;
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

		static void increaseBlockRefcount(Ref<Block> block);
		void        decreaseBlockRefcount(Ref<Block> block);

		[[nodiscard]]
		static auto newBlockReference(Ref<Block> block, u64 offset) -> Pointer;

		[[nodiscard]]
		static constexpr
			__attribute__((always_inline)) auto getPointerData(Pointer pointer, u64 size_bytes)
				-> base::ModRawView {
			if (pointer.block == nullptr) throw exceptions::VMNullPointerAccessException();
			if (pointer.block->deallocated) throw exceptions::VMUseAfterFreeException();
			if (pointer.offset + size_bytes > pointer.block->data.view.size())
				throw exceptions::VMOutOfBlockBoundsException();
			return { pointer.block->data.view.getBegin() + pointer.offset, size_bytes };
		}

		/** @brief Returns the block data from `pointer.offset` to the end of the block. */
		[[nodiscard]]
		static constexpr
			__attribute__((always_inline)) auto getRemainingPointerData(Pointer pointer)
				-> base::ModRawView {
			if (pointer.block == nullptr) throw exceptions::VMNullPointerAccessException();
			if (pointer.block->deallocated) throw exceptions::VMUseAfterFreeException();
			if (pointer.offset > pointer.block->data.view.size())
				throw exceptions::VMOutOfBlockBoundsException();
			return { pointer.block->data.view.getBegin() + pointer.offset,
				     pointer.block->data.view.size() - pointer.offset };
		}

		/**
		 * @brief Copies data pointed-to by `src` to `dst`.
		 * @note Assumes that the size of `type` is known at compile time and (implicitly)
		 * that it is the type of the blocks pointed-to by `dst` and `src`.
		 */
		auto copyPointedData(Pointer dst, Pointer src, TypeCRef type) -> void;

		auto destroyBlockReference(Pointer pointer) -> void;

		auto updatePointerAssignment(Pointer dst, Pointer src) -> Pointer;

		// ======================== Requests ========================

		[[nodiscard]]
		auto requestBlockID(Ref<Block> block) -> BlockID;

		[[nodiscard]]
		auto requestBlockData(BlockID id) -> base::RawView;

		[[nodiscard]]
		auto requestBlockType(BlockID id) -> TypeCRef;
	};
}
