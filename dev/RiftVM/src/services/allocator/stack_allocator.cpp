#include <supervisor/vcpu.hpp>

#include "stack_allocator.hpp"

namespace vm {
	Memory& StackAllocator::getMemory(VCPU& vcpu) {
		return vcpu.getData().get<Memory>();
	}
}
