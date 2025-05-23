#pragma once

#include "allocator/block_data.hpp"
#include "allocator/dummy_allocator.hpp"
#include "allocator/heap_allocator.hpp"
#include "block.hpp"
#include "frame.hpp"
#include "pointer.hpp"
#include "thread_stack.hpp"

#include <base/exceptions.hpp>
#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/raw_view.hpp>
#include <base/ref.hpp>
#include <base/stable_container.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>

#include <cstring>
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

	public:
		Memory() = default;

		using error = std::string;

		// =================== Used by executor ===================

		auto initializeFrameStack() -> Ref<ThreadStack>;

		auto allocateHeap(TypeCRef type) -> Ref<Block>;

		/**
		 * @brief Creates a block with externally managed data life-time.
		 */
		auto allocateDummy(TypeCRef type, Ref<std::byte> stack_pointer) -> Ref<Block>;

		void freeBlock(Ref<Block> block);

		void insertGlobalData(GlobalDataID id, TypeCRef type);

		/**
		 * @brief Returns a view of global data by the id.
		 */
		[[nodiscard]] constexpr __attribute__((always_inline)) auto getGlobalData(GlobalDataID id)
			-> base::ModRawView {
			std::lock_guard lock(mutex);
			return global_data.atMaybe(id).expect("Id not stored!").modView();
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
			if (pointer.block == nullptr) CORE_PANIC("Accessing null pointer");
			std::lock_guard lock(*pointer.block->mutex_ref);
			if (pointer.block->deallocated) CORE_PANIC("Data was freed");
			if (pointer.offset + size_bytes > pointer.block->data.view.size())
				CORE_PANIC("Accessing data out of bounds");
			return { pointer.block->data.view.getBegin() + pointer.offset, size_bytes };
		}

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
