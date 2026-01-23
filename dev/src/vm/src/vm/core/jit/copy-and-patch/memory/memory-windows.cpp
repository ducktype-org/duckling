#include "memory.hpp"

#if not _WIN32
	#error "Use only under windows"
#else

	#include <windows.h>

	#include <cassert>
	#include <cstddef>
	#include <iostream>

size_t get_page_size() { 
    static SYSTEM_INFO system_info = []() {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        return system_info;
    };
    return system_info.dwPageSize;
}

JitMemory JitMemory::allocate(size_t size) {
	assert(size % get_page_size() == 0);
	int        flags = MAP_ANONYMOUS | MAP_PRIVATE;
	std::byte* memory
		= reinterpret_cast<std::byte*>(VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE););

	return JitMemory{ .memory = memory, .size = size };
}

void JitMemory::mark_executable() {
	DWORD dummy;
	VirtualProtect(memory, size, PAGE_EXECUTE_READ, &dummy);
}

void JitMemory::free_jit_memory() {
    VirtualFree(memory, 0, MEM_RELEASE);
}

#endif  // _WIN32
