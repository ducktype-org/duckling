#include <core/process/memory/memory.hpp>

#include "stack_allocator.hpp"

namespace vm {
	StackAllocator::StackAllocator(Memory& memory): memory(memory) {}

	BlockID StackAllocator::makeArrayBlock(TypeCRef type, u64 length, base::ModRawView stack_ptr) {
		auto block_id = memory.reserveBlockID();
		std::memset(stack_ptr.getBegin(), 0, stack_ptr.size() * length);
		memory.makeBlock(block_id, Block(block_id, type, length, stack_ptr));
		return block_id;
	}

	void StackAllocator::deleteBlock(BlockID block_id) {
		memory.deleteBlock(block_id);
		memory.returnBlockID(block_id);
	}

	BlockID StackAllocator::makeTypeBlock(TypeCRef type, base::ModRawView data) {
		return makeArrayBlock(type, 1, data);
	}
}
