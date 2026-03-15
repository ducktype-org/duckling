#include "memory.hpp"

#if __unix__
	#include <base/except/exceptions.hpp>
	
	#include <sys/mman.h>
	#include <unistd.h>

	#include <cstddef>
	#include <functional>
	#include <iostream>

namespace vm::jit::cnp {
	inline size_t get_page_size() {
		static size_t page_size = std::invoke([]() {
			long result = sysconf(_SC_PAGESIZE);
			SYSTEM_CHECK(result != -1, "couldn't get the page size");
			return static_cast<size_t>(result);
		});

		return page_size;
	}

	JitMemory JitMemory::allocate(size_t size) {
		size = (size + get_page_size() - 1) / get_page_size() * get_page_size();
		CORE_ASSERT(size % get_page_size() == 0, "should be aligned to page size");
		int  flags = MAP_ANONYMOUS | MAP_PRIVATE;
		auto memory
			= reinterpret_cast<std::byte*>(mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0));

		SYSTEM_CHECK(memory != MAP_FAILED, "unable to allocate memory");
		return JitMemory{ .memory = memory, .size = size };
	}

	void JitMemory::mark_executable() {
		SYSTEM_CHECK(
			mprotect(memory, size, PROT_READ | PROT_EXEC) == 0, "unable to mark memory as executable"
		);
	}

	void JitMemory::free_jit_memory() {
		SYSTEM_CHECK(munmap(memory, size), "unable to free memory");
	}
}

#elif _WIN32
	#include <windows.h>

	#include <cstddef>
	#include <iostream>

namespace vm::jit::cnp {
	size_t get_page_size() {
		static SYSTEM_INFO system_info = []() {
			SYSTEM_INFO system_info;
			GetSystemInfo(&system_info);
			return system_info;
		};
		return system_info.dwPageSize;
	}

	JitMemory JitMemory::allocate(size_t size) {
		CORE_ASSERT(size % get_page_size() == 0, "should be aligned to page size");
		int        flags = MAP_ANONYMOUS | MAP_PRIVATE;
		std::byte* memory
			= reinterpret_cast<std::byte*>(VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE););

		return JitMemory{ .memory = memory, .size = size };
	}

	void JitMemory::mark_executable() {
		DWORD dummy;
		VirtualProtect(memory, size, PAGE_EXECUTE_READ, &dummy);
	}

	void JitMemory::free_jit_memory() { VirtualFree(memory, 0, MEM_RELEASE); }

}
#endif
