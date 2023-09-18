#include "stack_allocator.hpp"

#include <supervisor/vcpu.hpp>

namespace vm {
	Memory& StackAllocator::getMemory(VCPU& vcpu) {
		return vcpu.getData().get<Memory>();
	}
}
