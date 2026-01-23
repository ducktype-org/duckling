#pragma once

struct JitMemory {
	static JitMemory allocate(size_t size);

	void mark_executable();
	void free_jit_memory();

	std::byte* memory;
	size_t     size;
};
