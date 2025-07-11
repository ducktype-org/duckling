#include "memory.hpp"

#include "block.hpp"

#include <base/exceptions.hpp>
#include <base/raw_view.hpp>

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
		std::lock_guard lock(mutex);
		threads_frame_stacks.emplace_back();
		return &threads_frame_stacks.back();
	}

	auto Memory::allocateHeap(TypeCRef type) -> Ref<Block> {
		std::lock_guard lock(mutex);
		return createBlock(heap_allocator.allocate(type));
	}

	auto Memory::allocateDummy(TypeCRef type, Ref<std::byte> stack_pointer) -> Ref<Block> {
		std::lock_guard lock(mutex);
		return createBlock(dummy_allocator.allocate(type, stack_pointer));
	}

	void Memory::freeBlock(Ref<Block> block) {
		std::lock_guard lock(mutex);
		for (auto [offset, child]: block->children_blocks) freeBlock(child);

		// Parents reference their children so that they don't disappear on someone's pointer
		// destruction.
		if (block->parent)
			destroyReference(block);
		else
			block->data.allocator->deallocate(&block->data);

		block->deallocated = true;
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

	bool Memory::tryInsertGlobalData(GlobalDataID id, TypeCRef type) {
		std::lock_guard lock(mutex);
		if (!global_data.contains(id)) {
			auto             type_size = type->getSize();
			base::OwningView storage(new byte[type_size], type_size);
			global_data.put(id, std::move(storage));
			return true;
		}
		return false;
	}

	MRef<Block> Memory::getNestedViewBlock(Pointer parent_pointer, TypeCRef type) {
		CORE_ASSERT(!parent_pointer.isNull(), "Accessing null pointer");
		std::lock_guard lock(*parent_pointer.block->mutex_ref);
		if_opt_some(parent_pointer.block->children_blocks.atMaybe(parent_pointer.offset), nested) {
			if ((*nested)->data.element_type == type) return *nested;
		}
		return nullptr;
	}

	void Memory::setNestedViewBlock(Pointer parent_pointer, TypeCRef type) {
		CORE_ASSERT(!parent_pointer.isNull(), "Accessing null pointer");
		std::lock_guard lock(mutex);
		auto&           children = parent_pointer.block->children_blocks;
		if_opt_some(children.atMaybe(parent_pointer.offset), nested) {
			freeBlock(*nested);
			children.erase(parent_pointer.offset);
		}

		auto block_data         = parent_pointer.block->data;
		block_data.element_type = type;
		block_data.view         = getPointerData(parent_pointer, type->getSize());

		auto new_block = createBlock(block_data);  // @note createBlock nulls them bytes
		new_block->refcount++;  // so that the block does not disappear accidentally
		children.put(parent_pointer.offset, new_block);
	}

	auto Memory::copyPointedData(Pointer dst, Pointer src, TypeCRef type) -> void {
		CORE_ASSERT(!dst.isNull() && !src.isNull(), "Copying to/from null pointer");

		std::lock_guard lock_dst(*dst.block->mutex_ref);
		std::lock_guard lock_src(*src.block->mutex_ref);

		// Free child blocks.
		auto& dst_child_blocks = dst.getBlock()->children_blocks;
		for (auto iter = dst_child_blocks.lower_bound(dst.offset);
		     iter != dst_child_blocks.end() && iter->first < dst.offset + type->getSize();
		     iter = dst_child_blocks.erase(iter)) {
			freeBlock(iter->second);
		}
		// Copy the child blocks
		auto& src_child_blocks = src.getBlock()->children_blocks;
		for (auto iter = src_child_blocks.lower_bound(src.offset);
		     iter != src_child_blocks.end() && iter->first < src.offset + type->getSize();
		     ++iter) {
			auto    offset      = dst.offset + iter->first - src.offset;
			Pointer new_pointer = Pointer(dst.getBlock(), offset);
			setNestedViewBlock(new_pointer, iter->second->data.element_type);
			// @TODO We should go down the tree here...
		}

		// Copy the data itself
		auto dst_view = getPointerData(dst, type->getSize());
		auto src_view = getPointerData(src, type->getSize());
		std::memcpy(dst_view.getBegin(), src_view.getBegin(), type->getSize());
	}

	auto Memory::destroyBlockReference(Pointer pointer) -> void {
		if_opt_some(pointer.block.toOpt(), block) {
			std::lock_guard lock(*block->mutex_ref);
			destroyReference(block);
		}
	}

	auto Memory::setPointer(Pointer& dst, Pointer src) -> void {
		destroyBlockReference(dst);
		if_opt_some(src.block.toOpt(), block) {
			std::lock_guard lock(*block->mutex_ref);
			block->refcount++;
			dst = src;
		}
	}

	auto Memory::newBlockReference(Ref<Block> block, u64 offset) -> Pointer {
		std::lock_guard lock(*block->mutex_ref);
		block->refcount++;
		return { block, offset };
	}

	auto Memory::getBlockType(Ref<Block> block) -> TypeCRef {
		std::lock_guard lock(*block->mutex_ref);
		return block->data.element_type;
	}
}
