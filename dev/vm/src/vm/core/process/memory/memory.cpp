#include "memory.hpp"
#include <base/optional.hpp>

#include <base/exceptions.hpp>

#include <mutex>

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

    auto Memory::downcastableTo(Pointer object, TypeCRef type) -> bool {
        if_opt_some(object.block.toOpt(), block) {
			std::shared_lock lock(*block->shared_mutex);
            // This is guaranted to exist by static verification.
            TypeCRef real_object_type = reinterpret_cast<Type *>(block->data.view.getBegin());
            
            return real_object_type->inheritsFrom(type);
        }
        return false;
    }

	auto Memory::requestBlockIDs() -> std::vector<BlockID> {
		std::vector<BlockID> ids;
		for (auto& block: blocks)
			if (block.used) ids.push_back(block.id);
		return ids;
	}

	auto Memory::requestBlockData(BlockID id) -> base::RawView {
		return { getBlock(id)->data.view.getBegin(), getBlock(id)->data.view.size() };
	}

	auto Memory::requestBlockType(BlockID id) -> TypeCRef {
		return getBlock(id)->data.element_type;
	}
}
