#include "shadow_memory.hpp"

namespace vm {
	// Explicit instantiation for ShadowMemory
	template class IMemory<ShadowEntry>;
	template class IMemory<ShadowPointer>;
}
