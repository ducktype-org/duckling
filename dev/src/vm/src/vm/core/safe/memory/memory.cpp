#include "memory.hpp"

namespace vm {
	// Explicit instantiation for the real memory module
	template class IMemory<std::byte>;
}
