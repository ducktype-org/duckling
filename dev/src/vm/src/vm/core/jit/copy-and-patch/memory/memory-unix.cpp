#include "memory.hpp"

#if not __unix__
	#error "Use only under unix"
#else

	#include <sys/mman.h>
	#include <unistd.h>

	#include <cassert>
	#include <cstddef>
	#include <iostream>
	#include <functional>

[[noreturn]] void jit_error(const char* msg) {
	std::cerr << msg;
	std::exit(1);
}

inline size_t get_page_size() { 
	static size_t page_size = std::invoke([]() { 
		long result = sysconf(_SC_PAGESIZE);
		if (result == -1) {
			jit_error("couldn't get the page size");
		} else {
			return static_cast<size_t>(result);
		}
	});

	return page_size;
}

JitMemory JitMemory::allocate(size_t size) {
	size = (size + get_page_size() - 1) / get_page_size() * get_page_size();
	assert(size % get_page_size() == 0);
	int  flags = MAP_ANONYMOUS | MAP_PRIVATE;
	auto memory
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
