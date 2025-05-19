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

		// =================== Variant operations ===================

		MRef<Block> getNestedViewBlock(Ref<Block> variant_block, u64 offset, TypeCRef type) {
			std::unique_lock lock(mutex);
			if_opt_some(variant_block->children_blocks.atMaybe(offset), nested) {
				if (nested->data.element_type == type) return nested;
			}
			return nullptr;
		}

		/**
		 * @brief Creates new block at position (kind of variant.)
		 */
		void setNestedViewBlock(Ref<Block> parent_block, u64 offset, TypeCRef type) {
			std::unique_lock lock(mutex);
			auto&            children = parent_block->children_blocks;
			if_opt_some(children.atMaybe(offset), nested) {
				freeBlock(nested);
				children.erase(offset);
			}

			auto block_data         = parent_block->data;
			block_data.element_type = type;
			block_data.view
				= base::ModRawView(parent_block->data.view.getBegin() + offset, type->getSize());

			// This is done by `createBlock`
			// Set memory to 0.
			std::memset(block_data.view.getBegin(), 0, block_data.view.size());

			auto new_block = createBlock(block_data);
			new_block->refcount++;  // so that the block does not disappear accidentally
			children.put(offset, new_block);
		}

		bool variantHoldsType(Ref<Block> variant_block, TypeCRef type);

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
			if (pointer.block == nullptr) CORE_PANIC("Accessing null pointer");
			std::shared_lock lock(*pointer.block->shared_mutex);
			if (pointer.block->deallocated) CORE_PANIC("Data was freed");
			if (pointer.offset + size_bytes > pointer.block->data.view.size())
				CORE_PANIC("Accessing data out of bounds");
			return { pointer.block->data.view.getBegin() + pointer.offset, size_bytes };
		}

		auto copyPointerData(Pointer dst, Pointer src, TypeCRef type) -> void {
            CORE_ASSERT(!dst.isNull() && !src.isNull(), "Copying to/from null pointer");

			// Copy the child blocks
			auto& dst_child_blocks = dst.getBlock()->children_blocks;
			for (auto iter = dst_child_blocks.lower_bound(dst.offset);
			     iter != dst_child_blocks.end() && iter->first < dst.offset + type->getSize();
			     iter = dst_child_blocks.erase(iter)) {
				freeBlock(iter->second);
			}
			auto& src_child_blocks = src.getBlock()->children_blocks;
			for (auto iter = src_child_blocks.lower_bound(src.offset);
			     iter != src_child_blocks.end() && iter->first < src.offset + type->getSize();
			     ++iter) {
                auto offset = dst.offset + iter->first - src.offset;
                setNestedViewBlock(dst.getBlock(), offset, iter->second->data.element_type);
            }

			// Copy the data itself
			auto dst_view = getPointerData(dst, type->getSize());
			auto src_view = getPointerData(src, type->getSize());
			std::memcpy(dst_view.getBegin(), src_view.getBegin(), type->getSize());
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
