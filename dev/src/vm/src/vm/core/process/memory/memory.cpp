#include "memory.hpp"

#include "block.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/raw_view.hpp>

#include <vm/core/process/exceptions.hpp>
#include <vm/utils/interpret.hpp>

#include <iostream>

namespace vm {

	Ref<Block> Memory::createBlock(BlockData data) {
		std::memset(data.view.getBegin(), 0, data.view.size());

		if (free_ids.empty()) {
			auto id = BlockID(blocks.size());
			blocks.emplace_back(id, data);
			return &blocks.back();
		} else {
			BlockID id = free_ids.back();
			free_ids.pop_back();
			blocks[usize(id)] = Block(id, data);
			return &blocks[static_cast<u64>(id)];
		}
	}

	void Memory::deleteBlock(Ref<Block> block) {
		if (!block->deallocated) throw exceptions::VMFoundMemoryLeakException();
		free_ids.push_back(block->id);
	}

	Ref<Block> Memory::getBlock(BlockID id) {
		const auto block_index = static_cast<usize>(id);
		if (block_index >= blocks.size()) throw exceptions::VMOutOfBlockBoundsException();
		if (blocks[block_index].deallocated) throw exceptions::VMUseAfterFreeException();
		return &blocks[block_index];
	}

	auto Memory::initializeFrameStack() -> Ref<ThreadStack> {
		threads_frame_stacks.emplace_back();
		return &threads_frame_stacks.back();
	}

	auto Memory::allocateHeap(TypeCRef type) -> Ref<Block> {
		return createBlock(heap_allocator.allocate(type));
	}

	auto Memory::allocateDummy(TypeCRef type, Ref<std::byte> stack_pointer) -> Ref<Block> {
		return createBlock(dummy_allocator.allocate(type, stack_pointer));
	}

	auto Memory::dynTableAllocateHeapN(TypeCRef tbl_type, u64 n) -> Ref<Block> {
		auto inner_type = tbl_type->getInnerType().value();
		return createBlock(heap_allocator.dynTableAllocateN(tbl_type, inner_type, n));
	}

	auto Memory::dynTableReallocateBlockDataN(Ref<Block> block, u64 n) -> void {
		auto      tbl_type       = block->data.element_type;
		auto      inner_type     = tbl_type->getInnerType().value();
		BlockData new_block_data = heap_allocator.dynTableAllocateN(tbl_type, inner_type, n);
		auto      old_view_size  = block->data.view.size();
		auto      new_view_size  = new_block_data.view.size();

		Block mock_block{ BlockID{ 0 }, new_block_data };

		moveBlockDataAndEraseSuffix(&mock_block, block, std::min(old_view_size, new_view_size));

		heap_allocator.deallocate(&block->data);

		block->data = mock_block.data;
	}

	void Memory::freeBlockData(Ref<Block> block) {
		for (const auto child: block->children_blocks | std::views::values) freeBlockData(child);

		runDataDestructors(block);

		// Parents reference their children so that they don't disappear on someone's pointer
		// destruction.
		if (block->parent) {
			block->deallocated = true;
			decreaseBlockRefcount(block);
		} else {
			block->data.allocator->deallocate(&block->data);
			block->deallocated = true;
		}
	}

	auto Memory::requestBlockID(Ref<Block> block) -> BlockID { return block->id; }

	auto Memory::requestBlockData(BlockID id) -> base::RawView {
		return { getBlock(id)->data.view.getBegin(), getBlock(id)->data.view.size() };
	}

	auto Memory::requestBlockType(BlockID id) -> TypeCRef {
		return getBlock(id)->data.element_type;
	}

	bool Memory::tryInsertGlobalData(
		usize global_buffer_offset, usize global_block_idx, TypeCRef type
	) {
		if (MRef(global_data_blocks[global_block_idx]).toOpt().empty()) {
			auto type_size = type->getSize().asInt();
			CORE_ASSERT(
				global_buffer_offset + type_size <= global_data_buffer.size(),
				"Global buffer overflow: trying to insert global data of size {}, at offset {}, "
				"but buffer size is only {}",
				type_size,
				global_buffer_offset,
				global_data_buffer.size()
			);
			CORE_ASSERT(
				global_block_idx < global_data_blocks.size(),
				"Global blocks buffer overflow: trying to insert global block at index {}, but "
				"buffer "
				"size is only {}",
				global_block_idx,
				global_data_blocks.size()
			);
			auto block = allocateDummy(type, global_data_buffer.data() + global_buffer_offset);
			increaseBlockRefcount(block);
			global_data_blocks[global_block_idx] = block.get();
			return true;
		}
		return false;
	}

	void Memory::deinitGlobals() {
		try {
			// We are first freeing all the data and then decreasing the refcounts.
			// This is very important, because there might be links between the global variables,
			// and if we were to free them and decrease the refcount in the wrong order we might
			// throw a false-positive exception. This solution avoids this problem.

			for (const auto& block_ptr: global_data_blocks) freeBlockData(Ref(block_ptr));

			for (const auto& block_ptr: global_data_blocks) decreaseBlockRefcount(Ref(block_ptr));
		} catch (exceptions::VMFoundMemoryLeakException&) {
			std::cerr
				<< "Leak during global data deinitialization - e.g. there was a global pointer to "
				   "data, that was not freed.\n";
			throw;
		}
	}

	MRef<Block> Memory::getNestedViewBlock(Pointer parent_pointer, TypeCRef type) {
		if (parent_pointer.isNull()) throw exceptions::VMNullPointerAccessException();
		if_opt_some(parent_pointer.block->children_blocks.atMaybe(parent_pointer.offset), nested) {
			if ((*nested)->data.element_type == type) return *nested;
		}
		return nullptr;
	}

	void Memory::setNestedViewBlock(Pointer parent_pointer, TypeCRef type) {
		if (parent_pointer.isNull()) throw exceptions::VMNullPointerAccessException();
		auto& children = parent_pointer.block->children_blocks;
		if_opt_some(children.atMaybe(parent_pointer.offset), nested) {
			freeBlockData(*nested);
			children.erase(parent_pointer.offset);
		}

		auto block_data         = parent_pointer.block->data;
		block_data.element_type = type;
		block_data.view         = getPointerData(parent_pointer, type->getSize().asInt());

		auto new_block    = createBlock(block_data);  // @note createBlock nulls them bytes
		new_block->parent = parent_pointer.getBlock();
		increaseBlockRefcount(new_block);  // so that the block does not disappear accidentally
		children.put(parent_pointer.offset, new_block);
	}

	void Memory::copyBlocksRecursively(Ref<Block> block_dst, Ref<Block> block_src) {
		runDataCopyConstructors(block_dst);
		for (auto nested: block_src->children_blocks) {
			Pointer new_pointer{ block_dst, nested.first };
			setNestedViewBlock(new_pointer, nested.second->data.element_type);
			copyBlocksRecursively(
				new_pointer.getBlock()->children_blocks[nested.first], nested.second
			);
		}
	}

	void Memory::moveBlocksRecursively(Ref<Block> block_dst, Ref<Block> block_src) {
		for (auto nested: block_src->children_blocks) {
			Pointer new_pointer{ block_dst, nested.first };
			setNestedViewBlock(new_pointer, nested.second->data.element_type);
			moveBlocksRecursively(
				new_pointer.getBlock()->children_blocks[nested.first], nested.second
			);
		}
	}

	void Memory::moveBlockDataAndEraseSuffix(Ref<Block> dst, Ref<Block> src, usize byte_count) {
		// Free all child blocks on suffix.
		auto& dst_child_blocks = dst->children_blocks;
		for (auto iter = dst_child_blocks.lower_bound(0); iter != dst_child_blocks.end();
		     iter      = dst_child_blocks.erase(iter)) {
			freeBlockData(iter->second);
		}

		// Copy the child blocks.
		auto& src_child_blocks = src->children_blocks;
		for (auto iter = src_child_blocks.lower_bound(0);
		     iter != src_child_blocks.end() && iter->first < byte_count;
		     ++iter) {
			auto    offset = iter->first;
			Pointer new_pointer{ dst, offset };
			setNestedViewBlock(new_pointer, iter->second->data.element_type);
			moveBlocksRecursively(new_pointer.getBlock()->children_blocks[offset], iter->second);
		}

		// Copy the data itself.
		// @note: We are not running destructors or copy-constructors
		// because the data being is "moved".
		std::memcpy(dst->data.view.getBegin(), src->data.view.getBegin(), byte_count);
	}

	auto Memory::copyPointedData(Pointer dst, Pointer src, TypeCRef type) -> void {
		// When copying with this or any other function we need to first free the previous
		// data and call the data destructors, then run copy constructors only on the copied data parts.

		if (dst.isNull() || src.isNull()) throw exceptions::VMNullPointerCopyException();


		// Free child blocks.
		auto& dst_child_blocks = dst.getBlock()->children_blocks;
		for (auto iter = dst_child_blocks.lower_bound(dst.offset);
		     iter != dst_child_blocks.end() && iter->first < dst.offset + type->getSize().asInt();
		     iter = dst_child_blocks.erase(iter)) {
			freeBlockData(iter->second);
		}

		const auto dst_view = getPointerData(dst, type->getSize().asInt());
		const auto src_view = getPointerData(src, type->getSize().asInt());
		runDataDestructors(dst_view, type);

		// Copy the child blocks
		auto& src_child_blocks = src.getBlock()->children_blocks;
		for (auto iter = src_child_blocks.lower_bound(src.offset);
		     iter != src_child_blocks.end() && iter->first < src.offset + type->getSize().asInt();
		     ++iter) {
			auto    offset      = dst.offset + iter->first - src.offset;
			Pointer new_pointer = Pointer(dst.getBlock(), offset);
			setNestedViewBlock(new_pointer, iter->second->data.element_type);
			copyBlocksRecursively(new_pointer.getBlock()->children_blocks[offset], iter->second);
		}

		// Copy the data itself
		std::memcpy(dst_view.getBegin(), src_view.getBegin(), type->getSize().asInt());
		runDataCopyConstructors(dst_view, type);
	}

	auto Memory::destroyBlockReference(Pointer pointer) -> void {
		if_opt_some(pointer.block.toOpt(), block) { decreaseBlockRefcount(block); }
	}

	auto Memory::updatePointerAssignment(Pointer dst, Pointer src) -> Pointer {
		if (dst.block != src.block) {
			// Decrease the dst block's refcount before assigning the new block
			destroyBlockReference(dst);
			if_opt_some(src.block.toOpt(), block) { increaseBlockRefcount(block); }
		}
		return src;
	}

	auto Memory::newBlockReference(Ref<Block> block, u64 offset) -> Pointer {
		increaseBlockRefcount(block);
		return { block, offset };
	}

	auto Memory::getBlockType(Ref<Block> block) -> TypeCRef { return block->data.element_type; }

	void Memory::increaseBlockRefcount(Ref<Block> block) { block->refcount++; }

	void Memory::decreaseBlockRefcount(Ref<Block> block) {
		CORE_ASSERT(
			block->refcount > 0, "Deleting an unreferenced block"
		);  // This should never be possible, even in a faulty program
		if (--block->refcount == 0) deleteBlock(block);
	}

	void Memory::runDataDestructors(Ref<Block> block) {
		iterateOverDataAndExecute(block, &Memory::runObjectDestructor);
	}

	void Memory::runDataDestructors(base::ModRawView data, TypeCRef type) {
		iterateOverDataAndExecute(data, type, &Memory::runObjectDestructor);
	}

	void Memory::runDataCopyConstructors(Ref<Block> block) {
		iterateOverDataAndExecute(block, &Memory::runObjectCopyConstructor);
	}

	void Memory::runDataCopyConstructors(base::ModRawView data, TypeCRef type) {
		iterateOverDataAndExecute(data, type, &Memory::runObjectCopyConstructor);
	}

	void Memory::iterateOverDataAndExecute(
		Ref<Block> block, void (Memory::*callback)(base::ModRawView data, TypeCRef type)
	) {
		iterateOverDataAndExecute(block->data.view, block->data.element_type, callback);
	}

	void Memory::iterateOverDataAndExecute(
		const base::ModRawView data,
		const TypeCRef         type,
		void (Memory::*callback)(base::ModRawView data, TypeCRef type)
	) {
		if (type->getKind() != Type::Kind::DynamicTable) {
			// Only types other than dynamic_table can be next to each other.
			// Callback on the first object
			(this->*callback)(base::ModRawView{ data.getBegin(), type->getSize().asInt() }, type);
		}

		switch (type->getKind()) {
		case Type::Kind::Primitive:
		case Type::Kind::Function:
		case Type::Kind::Opaque:
		case Type::Kind::Variant:
		case Type::Kind::Pointer:
			break;
		case Type::Kind::DynamicTable:
		case Type::Kind::FixedSizeTable: {
			const auto inner_type = type->getInnerType().value();
			const auto inner_size = inner_type->getSize().asInt();
			for (usize begin = 0; begin < data.size(); begin += inner_size)
				(this->*callback)(
					base::ModRawView{ data.getBegin() + begin, inner_size }, inner_type
				);
			break;
		}
		case Type::Kind::Data: {
			// Iterate over data's fields
			for (const auto& fields = **type->getFields(); auto [offset, tp]: fields)
				(this->*callback)(
					base::ModRawView{ data.getBegin() + offset.asInt(), tp->getSize().asInt() }, tp
				);
			break;
		}
		default:
			CORE_PANIC("Handling default");
		}

		if (type->getKind() != Type::Kind::DynamicTable) {
			// In case we were given a slice of a table with multiple objects of the same type laying
			// next to each other, then iterate over those as well.
			// Here we start from the second, since the first one was handled above
			for (auto next_item = type->getSize().asInt(); next_item < data.size();
			     next_item += type->getSize().asInt()) {
				iterateOverDataAndExecute(
					base::ModRawView{ data.getBegin() + next_item, type->getSize().asInt() },
					type,
					callback
				);
			}
		}
	}

	void Memory::runObjectDestructor(base::ModRawView data, TypeCRef type) {
		switch (type->getKind()) {
		case Type::Kind::Pointer: {
			const auto ptr = safeReadPointerBytes<Pointer>(data.getBegin());
			destroyBlockReference(ptr);
			break;
		}
		case Type::Kind::Primitive:
		case Type::Kind::Function:
		case Type::Kind::Opaque:
		case Type::Kind::DynamicTable:
		case Type::Kind::FixedSizeTable:
		case Type::Kind::Data:
		case Type::Kind::Variant:
			// There is nothing to do with variant, data and tables, because the data should be
			// already deleted thanks to the nested blocks structure, that deletes the nested
			// block's data first.
			break;
		default:
			CORE_PANIC("Handling default");
		}
	}

	void Memory::runObjectCopyConstructor(base::ModRawView data, TypeCRef type) {
		switch (type->getKind()) {
		case Type::Kind::Pointer: {
			const auto ptr = safeReadPointerBytes<Pointer>(data.getBegin());
			if_opt_some(ptr.block.toOpt(), block) increaseBlockRefcount(block);
			break;
		}
		case Type::Kind::Primitive:
		case Type::Kind::Function:
		case Type::Kind::Opaque:
		case Type::Kind::DynamicTable:
		case Type::Kind::FixedSizeTable:
		case Type::Kind::Data:
		case Type::Kind::Variant:
			// There is nothing to do with variant, data and tables, because the data should be
			// already copied thanks to the nested blocks structure, that deletes the nested block's
			// data first.
			break;
		default:
			CORE_PANIC("Handling default");
		}
	}

	bool Memory::validateMemoryState() const {
#define TEST_HERE(test)                                              \
	if (test) {                                                      \
		std::cerr << #test ", BlockID=" << block.id.asInt() << "\n"; \
		return false;                                                \
	}
		for (const auto& block: blocks) {
			TEST_HERE(block.refcount != 0)
			TEST_HERE(!block.deallocated)
		}
		return true;
	}

	void Memory::reallocateBufferForGlobals(usize global_count, usize buffer_size) {
		global_data_buffer.resize(buffer_size);
		global_data_blocks.resize(global_count);
	}

	GlobalBufferPointers Memory::getGlobalDataMemory() {
		return { .data_buffer_base   = global_data_buffer.data(),
			     .blocks_buffer_base = global_data_blocks.data() };
	}
}
