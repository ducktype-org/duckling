#include "memory.hpp"

#include "block.hpp"

#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <base/raw_view.hpp>

#include <vm/core/process/exceptions.hpp>

#include <iostream>
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
		if (block->refcount == 0) throw exceptions::VMUnreferencedBlockDeletionException();
		if (--block->refcount == 0) deleteBlock(block);
	}

	Ref<Block> Memory::getBlock(BlockID id) {
		if (static_cast<u64>(id) >= blocks.size()) throw exceptions::VMOutOfBlockBoundsException();
		if (!blocks[usize(id)].used) exceptions::VMUseAfterFreeException();
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

	bool Memory::tryInsertGlobalData(
		GlobalDataID id, TypeCRef type, base::Optional<u64> initial_value
	) {
		std::lock_guard lock(mutex);
		if (!global_data.contains(id)) {
			size_t type_size = type->getSize();
			byte*  raw       = new byte[type_size];

			base::OwningView storage(raw, type_size);
			global_blocks.put(id, allocateDummy(type, storage.modView().getBegin()));
			
			if (initial_value.has_value())
				std::memcpy(storage.modView().getBegin(), &initial_value.value(), type_size);

			global_data.put(id, std::move(storage));
			return true;
		}
		return false;
	}

	MRef<Block> Memory::getNestedViewBlock(Pointer parent_pointer, TypeCRef type) {
		if (parent_pointer.isNull()) throw exceptions::VMNullPointerAccessException();
		std::lock_guard lock(*parent_pointer.block->mutex_ref);
		if_opt_some(parent_pointer.block->children_blocks.atMaybe(parent_pointer.offset), nested) {
			if ((*nested)->data.element_type == type) return *nested;
		}
		return nullptr;
	}

	void Memory::setNestedViewBlock(Pointer parent_pointer, TypeCRef type) {
		if (parent_pointer.isNull()) throw exceptions::VMNullPointerAccessException();
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
		if (dst.isNull() || src.isNull()) throw exceptions::VMNullPointerCopyException();

		std::lock_guard lock_dst(*dst.block->mutex_ref);
		std::lock_guard lock_src(*src.block->mutex_ref);

		// Free child blocks.
		auto& dst_child_blocks = dst.getBlock()->children_blocks;
		for (auto iter = dst_child_blocks.lower_bound(dst.offset);
		     iter != dst_child_blocks.end() && iter->first < dst.offset + type->getSize();
		     iter = dst_child_blocks.erase(iter)) {
			freeBlock(iter->second);
		}

		auto copy_blocks_recursivly
			= [this](const auto& self, Ref<Block> block_dst, Ref<Block> block_src) -> void {
			for (auto nested: block_src->children_blocks) {
				Pointer new_pointer(block_dst, nested.first);
				setNestedViewBlock(new_pointer, nested.second->data.element_type);
				self(self, new_pointer.getBlock()->children_blocks[nested.first], nested.second);
			}
		};

		// Copy the child blocks
		auto& src_child_blocks = src.getBlock()->children_blocks;
		for (auto iter = src_child_blocks.lower_bound(src.offset);
		     iter != src_child_blocks.end() && iter->first < src.offset + type->getSize();
		     ++iter) {
			auto    offset      = dst.offset + iter->first - src.offset;
			Pointer new_pointer = Pointer(dst.getBlock(), offset);
			setNestedViewBlock(new_pointer, iter->second->data.element_type);
			copy_blocks_recursivly(
				copy_blocks_recursivly, new_pointer.getBlock()->children_blocks[offset], iter->second
			);
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
		if_opt_some(dst.block.toOpt(), block) {
			std::lock_guard lock(*block->mutex_ref);
			destroyBlockReference(dst);
		}
		if_opt_some(src.block.toOpt(), block) {
			std::lock_guard lock(*block->mutex_ref);
			block->refcount++;
		}
		dst = src;
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
