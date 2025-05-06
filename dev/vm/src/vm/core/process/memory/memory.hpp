#pragma once

#include "allocator/block_data.hpp"
#include "allocator/heap_allocator.hpp"
#include "allocator/stack_allocator.hpp"
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
#include <shared_mutex>

namespace vm {
	/**
	 * @brief A memory module for a process.
	 * All of process'es memory - thread stacks (thread local data) and global data is stored here.
     */
	class Memory final {
	private:
		mutable std::shared_mutex mutex;
		HeapAllocator             heap_allocator;
		StackAllocator            stack_allocator;

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

		auto allocateStack(TypeCRef type, Ref<std::byte> stack_pointer) -> Ref<Block>;

		void freeBlock(Ref<Block> block);

		void insertGlobalData(GlobalDataID id, TypeCRef type);

		/**
		 * @brief Returns a view of global data by the id.
		 */
		[[nodiscard]]
		__attribute__((always_inline)) auto getGlobalData(GlobalDataID id) -> base::ModRawView {
			return global_data.atMaybe(id).expect("Id not stored!").modView();
		}

		// =================== Block operations ===================

		[[nodiscard]]
		static auto getBlockType(Ref<Block> block) -> TypeCRef {
			std::shared_lock lock(*block->shared_mutex);
			return block->data.element_type;
		}

		// ======================== Pointers ========================

		[[nodiscard]]
		static auto getPointer(Ref<Block> block) -> Pointer {
			std::unique_lock lock(*block->shared_mutex);
			block->refcount++;
			return { block, 0 };
		}

		// @todo panics slows down the execution of the code in executor
		// we should implement entirely different error handling (maybe exception free)
		[[nodiscard]]
		static __attribute__((always_inline)) auto getPointerData(Pointer pointer, u64 size_bytes)
			-> base::ModRawView {
			std::shared_lock lock(*pointer.block->shared_mutex);
			if (pointer.block == nullptr) CORE_PANIC("Accessing null pointer");
			if (pointer.block->deallocated) CORE_PANIC("Data was freed");
			if (pointer.offset + size_bytes > pointer.block->data.view.size())
				CORE_PANIC("Accessing data out of bounds");
			return { pointer.block->data.view.getBegin() + pointer.offset, size_bytes };
		}

		auto destroyPointer(Pointer pointer) -> void {
			if_opt_some(pointer.block.toOpt(), block) {
				std::unique_lock lock(*block->shared_mutex);
				destroyReference(block);
			}
		}

		auto setPointer(Pointer& dst, Pointer src) -> void {
			destroyPointer(dst);
			if_opt_some(src.block.toOpt(), block) {
				std::unique_lock lock(*block->shared_mutex);
				block->refcount++;
				dst = src;
			}
		}

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
