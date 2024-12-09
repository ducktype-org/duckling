#include <core/process/vmprocess.hpp>
#include <core/process/memory/memory.hpp>

#include "allocator.hpp"

namespace vm {
	Allocator::Allocator(Memory& memory): memory(memory) {}

	BlockID Allocator::makeTypeBlock(TypeCRef type) { return makeArrayBlock(type, 1); }

	BlockID Allocator::makeArrayBlock(TypeCRef type, u64 length) {
		auto  block_id = memory.reserveBlockID();
		byte* data     = new byte[type->getSize() * length];
		std::memset(data, 0, type->getSize() * length);
		memory.makeBlock(
			block_id,
			Block(block_id, type, length, base::ModRawView(data, type->getSize() * length))
		);
		return block_id;
	}

	void Allocator::deleteBlock(BlockID block_id) {
		delete[] memory.getBlock(block_id)->rawPointer().getBegin();
		memory.deleteBlock(block_id);
		memory.returnBlockID(block_id);
	}

}
