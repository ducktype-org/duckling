#include <supervisor/vcpu.hpp>

#include "stack_allocator.hpp"

namespace vm {
	Memory& StackAllocator::getMemory(Process& vcpu) { return vcpu.getData().get<Memory>(); }
}
