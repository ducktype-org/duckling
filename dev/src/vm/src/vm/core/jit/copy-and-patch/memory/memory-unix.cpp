#include "memory.hpp"

#if not __unix__
	#error "Use only under windows"
#else

#include <sys/mman.h>
#include <unistd.h>

#include <cassert>
#include <cstddef>
#include <iostream>

void jit_error(char* msg) {
	std::cerr << msg;
	std::exit(1);
}

inline size_t get_page_size() { return sysconf(_SC_PAGESIZE); }

JitMemory JitMemory::allocate(size_t size) {
	assert(size % get_page_size() == 0);
	int        flags = MAP_ANONYMOUS | MAP_PRIVATE;
	std::byte* memory
		= reinterpret_cast<std::byte*>(mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0));

	if (memory == MAP_FAILED) jit_error("unable to allocate memory");
	return JitMemory{ .memory = memory, .size = size };
}

void JitMemory::mark_executable() {
	int failed = mprotect(memory, size, PROT_READ | PROT_EXEC);
	if (failed) jit_error("unable to mark memory as executable");
}

void JitMemory::free_jit_memory() {
	int failed = munmap(memory, size);
	if (failed) jit_error("unable to free memory");
}

#endif
