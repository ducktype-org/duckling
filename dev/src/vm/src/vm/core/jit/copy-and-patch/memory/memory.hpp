#pragma once

#include <cstddef>

namespace vm::jit::cnp {

struct JitMemory {
	static JitMemory allocate(size_t size);

	void mark_executable();
	void free_jit_memory();

	template<class Func>
	const Func* into_func() const {
		return reinterpret_cast<const Func*>(memory);
	}

	std::byte* memory;
	size_t     size;
};

}
