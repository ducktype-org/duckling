#include <supervisor/vcpu.hpp>

#include "allocator.hpp"

namespace vm {
	Memory& Allocator::getMemory(VCPU& vcpu) { return vcpu.getData().get<Memory>(); }

	BlockId Allocator::makeTypeBlock(TypeCRef type) {
		auto  block_id = memory.reserveBlockID();
		byte* data     = new byte[type->getSize()];
		std::memset(data, 0, type->getSize());
		memory.makeBlock(block_id, Block(block_id, type, base::ModRawView(data, type->getSize())));
		return block_id;
	}

	BlockId Allocator::makeArrayBlock(TypeCRef type, u64 length) {
		auto  block_id = memory.reserveBlockID();
		byte* data     = new byte[type->getSize() * length];
		std::memset(data, 0, type->getSize() * length);
		memory.makeBlock(
			block_id,
			Block(block_id, type, length, base::ModRawView(data, type->getSize() * length))
		);
		return block_id;
	}

	void Allocator::deleteBlock(BlockId block_id) {
		delete[] memory.getBlock(block_id).value()->rawPointer().getBegin();
		memory.deleteBlock(block_id);
		memory.returnBlockID(block_id);
	}
}
