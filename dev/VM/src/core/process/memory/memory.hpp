#pragma once

#include <deque>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <cstring>
#include <base/ints.hpp>
#include <base/ref.hpp>
#include <base/exceptions.hpp>
#include <base/raw_view.hpp>
#include <base/stable_container.hpp>
#include <base/maps.hpp>

#include "allocator/block_data.hpp"
#include "block.hpp"
#include "pointer.hpp"
#include "code_data/frame.hpp"
#include "allocator/heap_allocator.hpp"
#include "allocator/stack_allocator.hpp"
#include "core/process/type_metadata/definitions.hpp"

namespace vm {
	class Memory {
	private:
		static constexpr u64 FRAMES_LENGTH = 16'384;
		static constexpr u64 STACK_LENGTH  = FRAMES_LENGTH * 256;

		mutable std::shared_mutex mutex_;
		HeapAllocator             heap_allocator;
		StackAllocator            stack_allocator;

		std::deque<std::vector<Frame>> threads_executor_frame_stack;
		/**
		 * @brief Continous block of memory, that is used for the call stack.
		 * Here each stack frame is composed of: "arg_stack", local_stack". The "arg_stack" is used
		 * for arguments passed to the function, and the "local_stack" is used for local variables.
		 * [arg_stack(1) | local_stack(1) | arg_stack(1) | local_stack(2) | ...]
		 * When preparing for a new function call, the arguments are placed
		 * exactly after the local variables of the current function, so in the "arg_stack"
		 * of the new frame.
		 */
		std::deque<std::vector<std::byte>> threads_executor_local_stack;

		std::deque<Block> blocks = {};
		// @TODO: stable vector doesn't have pop_back method, so for now we use deque
		std::deque<BlockID> free_ids = {};

		[[nodiscard]]
		Ref<Block> createBlock(BlockData data);

		void deleteBlock(Ref<Block> block);

		static void createReference(Ref<Block> block);

		void destroyReference(Ref<Block> block);

		[[nodiscard]]
		Ref<Block> getBlock(BlockID id);

	public:
		Memory() = default;

		using error = std::string;

		// =================== Used by executor ===================

		auto initializeFrameStack()
			-> std::pair<Ref<std::vector<Frame>>, Ref<std::vector<std::byte>>>;

		auto allocateHeap(TypeCRef type) -> Ref<Block>;

		auto allocateStack(TypeCRef type, Ref<std::byte> stack_pointer) -> Ref<Block>;

		void freeBlock(Ref<Block> block);

		// =================== Block operations ===================

		[[nodiscard]]
		static inline auto getBlockType(Ref<Block> block) -> TypeCRef {
			std::shared_lock lock(*block->shared_mutex);
			return block->data.element_type;
		}

		// ======================== Pointers ========================

		[[nodiscard]]
		static inline auto getPointer(Ref<Block> block) -> Pointer {
			std::unique_lock lock(*block->shared_mutex);
			createReference(block);
			return { block, 0 };
		}

		// @todo panics slows down the execution of the code in executor
		// we should implement entirely different error handling (maybe exception free)
		[[nodiscard]]
		static __attribute__((always_inline)
		) inline auto getPointerData(Pointer pointer, u64 size_bytes) -> base::ModRawView {
			std::shared_lock lock(*pointer.block->shared_mutex);
			if (pointer.block == nullptr) CORE_PANIC("Accessing null pointer");
			if (pointer.block->deallocated) CORE_PANIC("Data was freed");
			if (pointer.offset + size_bytes > pointer.block->data.view.size())
				CORE_PANIC("Accessing data out of bounds");
			return { pointer.block->data.view.getBegin() + pointer.offset, size_bytes };
		}

		inline auto destroyPointer(Pointer pointer) -> void {
			std::unique_lock lock(*pointer.block->shared_mutex);
			destroyReference(pointer.block);
		}

		// ======================== Requests ========================

		[[nodiscard]]
		auto requestBlockIDs() -> base::StableVector<BlockID>;

		[[nodiscard]]
		auto requestBlockData(BlockID id) -> base::RawView;

		[[nodiscard]]
		auto requestBlockType(BlockID id) -> TypeCRef;
	};
}
