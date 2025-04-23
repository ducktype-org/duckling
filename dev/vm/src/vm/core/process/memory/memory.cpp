#include "memory.hpp"

#include "block.hpp"

#include <base/exceptions.hpp>
#include <base/raw_view.hpp>

#include <algorithm>
#include <mutex>
#include <ranges>

namespace vm {

	Ref<Block> Memory::createBlock(BlockData data) {
		std::memset(data.view.getBegin(), 0, data.view.size());

		if (free_ids.empty()) {
			auto id = BlockID(blocks.size());
			blocks.emplace_back(id, data, &mutex);
			return &blocks.back();
		} else {
			BlockID id = free_ids.back();
			free_ids.pop_back();
			blocks[usize(id)] = Block(id, data, &mutex);
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

	Ref<Block> Memory::getBlock(BlockID id) {
		if (static_cast<u64>(id) >= blocks.size()) CORE_PANIC("Accessing block out of bounds");
		if (!blocks[usize(id)].used) CORE_PANIC("Accessing freed block");
		return &blocks[static_cast<u64>(id)];
	}

	auto Memory::initializeFrameStack() -> Ref<ThreadStack> {
		std::unique_lock lock(mutex);
		threads_frame_stacks.emplace_back();
		return &threads_frame_stacks.back();
	}

	auto Memory::allocateHeap(TypeCRef type) -> Ref<Block> {
		std::unique_lock lock(mutex);
		return createBlock(heap_allocator.allocate(type));
	}

	auto Memory::allocateStack(TypeCRef type, Ref<std::byte> stack_pointer) -> Ref<Block> {
		std::unique_lock lock(mutex);
		return createBlock(stack_allocator.allocate(type, stack_pointer));
	}

	void Memory::freeBlock(Ref<Block> block) {
		std::unique_lock lock(mutex);
		block->deallocated = true;
		block->data.allocator->deallocate(&block->data);
		if (block->refcount == 0) deleteBlock(block);
	}

	auto Memory::requestBlockIDs() -> std::vector<BlockID> {
		std::vector<BlockID> ids;
		for (auto& block: blocks)
			if (block.used) ids.push_back(block.id);
		return ids;
	}

	auto Memory::requestBlockID(Ref<Block> block) -> BlockID { return block->id; }

	auto Memory::requestBlockData(BlockID id) -> base::RawView {
		return { getBlock(id)->data.view.getBegin(), getBlock(id)->data.view.size() };
	}

	auto Memory::requestBlockType(BlockID id) -> TypeCRef {
		return getBlock(id)->data.element_type;
	}

	void Memory::insertGlobalData(usize id, TypeCRef type) {
		CORE_ASSERT(!global_data.contains(id), "Duplicated global data id!");

		auto             type_size = type->getSize();
		base::OwningView storage(new byte[type_size], type_size);
		global_data.put(id, std::move(storage));
	}
}
