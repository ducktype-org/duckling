#include <iostream>
#include <base/exceptions.hpp>
#include <mutex>
#include "memory.hpp"

namespace vm {

	Ref<Block> Memory::createBlock(BlockData data) {
		std::memset(data.view.getBegin(), 0, data.view.size());

		if (free_ids.empty()) {
			auto id = BlockID(blocks.size());
			blocks.emplace_back(id, data, &mutex_);
			return &blocks.back();
		} else {
			BlockID id = free_ids.back();
			free_ids.pop_back();
			blocks[usize(id)] = Block(id, data, &mutex_);
			return &blocks[static_cast<u64>(id)];
		}
	}

	void Memory::deleteBlock(Ref<Block> block) {
		block->used = false;
		free_ids.push_back(block->id);
	}

	void Memory::destroyReference(Ref<Block> block) {
		if (block->refcount == 0) CORE_PANIC("Tried deleting a reference to an unreferenced block");
		if (--block->refcount == 0) deleteBlock(block);
	}

	void Memory::createReference(Ref<Block> block) { block->refcount++; }

	Ref<Block> Memory::getBlock(BlockID id) {
		if (static_cast<u64>(id) >= blocks.size()) CORE_PANIC("Accessing block out of bounds");
		if (!blocks[usize(id)].used) CORE_PANIC("Accessing freed block");
		return &blocks[static_cast<u64>(id)];
	}

	auto Memory::initializeFrameStack()
		-> std::pair<Ref<std::vector<Frame>>, Ref<std::vector<std::byte>>> {
		std::unique_lock lock(mutex_);
		threads_executor_frame_stack.emplace_back(FRAMES_LENGTH);
		threads_executor_local_stack.emplace_back(STACK_LENGTH);
		return { &threads_executor_frame_stack.back(), &threads_executor_local_stack.back() };
	}

	auto Memory::allocateHeap(TypeCRef type) -> Ref<Block> {
		std::unique_lock lock(mutex_);
		return createBlock(heap_allocator.allocate(type));
	}

	auto Memory::allocateStack(TypeCRef type, Ref<std::byte> stack_pointer) -> Ref<Block> {
		std::unique_lock lock(mutex_);
		return createBlock(stack_allocator.allocate(type, stack_pointer));
	}

	void Memory::freeBlock(Ref<Block> block) {
		std::unique_lock lock(mutex_);
		block->deallocated = true;
		block->data.allocator->deallocate(&block->data);
		if (block->refcount == 0) deleteBlock(block);
	}

	auto Memory::requestBlockIDs() -> base::StableVector<BlockID> {
		base::StableVector<BlockID> ids;
		for (auto& block: blocks)
			if (block.used) ids.pushBack(block.id);
		return ids;
	}

	auto Memory::requestBlockData(BlockID id) -> base::RawView {
		return { getBlock(id)->data.view.getBegin(), getBlock(id)->data.view.size() };
	}

	auto Memory::requestBlockType(BlockID id) -> TypeCRef {
		return getBlock(id)->data.element_type;
	}
}
