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
		// std::unique_lock lock(mutex);
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

	void Memory::insertGlobalData(GlobalDataID id, TypeCRef type) {
		if (!global_data.contains(id)) {
			auto             type_size = type->getSize();
			base::OwningView storage(new byte[type_size], type_size);
			global_data.put(id, std::move(storage));
		}
	}

	// Ref<Block> Memory::variantChangeType(Ref<Block> variant_block, TypeCRef new_type) {
	// 	CORE_ASSERT(variant_block->variant_data, "Block is not a variant block");
	// 	CORE_ASSERT(!variant_block->deallocated, "Variant operation on deallocated block");
	// 	std::unique_lock lock(mutex);

	// 	// auto&            variant_data = variant_block->variant_data.value();
	// 	// if (variant_data.parent) {
	// 	// 	// We have to remove our-selves from parent
	// 	// 	// @TODO: Improve speed of this operation.
	// 	// 	auto& parent_children = variant_data.parent->variant_data->children_blocks;
	// 	// 	for (auto it = parent_children.begin(); it != parent_children.end(); it++) {
	// 	// 		if (*it == variant_block) {
	// 	// 			parent_children.erase(it);
	// 	// 			break;
	// 	// 		}
	// 	// 	}

	// 	// } else {
	// 	// 	// Nothing?
	// 	// }

	// 	// Create a new block that points to the same memory.

	// 	// Invalidate children.
	// 	// A child may be a field inside a structure
	// 	// that is of a variant type, or a variant within variant.
	// 	// A child may already be deallocated, but the reference was not
	// 	// removed from children list for the sake of efficiency.
	// 	std::vector<Ref<Block>> stack;
	// 	stack.insert(variant_block->variant_data->children_blocks);
	// 	while (!stack.empty()) {
	// 		auto front = stack.back();
	// 		stack.pop_back();
	// 		for (auto& [offset, child]: front->variant_data->children_blocks)
	// 			if (!child->deallocated) stack.push_back(child);
	// 		destroyReference(front);
	// 		freeBlock(front);
	// 	}

	// 	return variantDataReference(variant_block, 0, new_type);
	// 	// Whenever we create a child we have to mark it referenced.
	// 	// @TODO: Taking a pointer to variant must work.
	// 	// GENERALLY speaking: Main block has a variant type. Then we can have inner offset=0, which
	// 	// has inner type and so on.
	// 	//
	// 	// New idea -
	// }

	// bool Memory::variantHoldsType(Ref<Block> variant_block, TypeCRef type) {
	// 	CORE_ASSERT(variant_block->variant_data, "Not a variant");
	// 	return variant_block->data.element_type == type;
	// }

	// Ref<Block> Memory::variantDataReference(
	// 	Ref<Block> variant_block, u64 variant_offset, TypeCRef type
	// ) {
	// 	auto& children = variant_block->variant_data->children_blocks;
	// 	if (children.contains(offset)) return children[offset];
	// 	auto data         = variant_block->data;
	// 	data.element_type = type;
	// 	createBlock(BlockData data)
	// }
}
