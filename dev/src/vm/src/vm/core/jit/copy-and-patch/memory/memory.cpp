#include "memory.hpp"

#include <fstream>
#include <utility>

namespace vm::jit::cnp {
	void JitFuncMemory::dump(const char* filename) {
		std::ofstream file{ filename, std::ios::binary };

		for (std::byte byte: span()) file << std::to_underlying(byte);
	}
}

#if __unix__
	#include <sys/mman.h>
	#include <unistd.h>

	#include <base/except/exceptions.hpp>

	#include <cstddef>
	#include <functional>
	#include <iostream>

namespace vm::jit::cnp {
	inline usize getPageSize() {
		static usize page_size = std::invoke([]() {
			long result = sysconf(_SC_PAGESIZE);
			CORE_ASSERT_SYSCALL(result != -1, "couldn't get the page size");
			return static_cast<usize>(result);
		});

		return page_size;
	}

	JitFuncMemory JitFuncMemory::allocate(usize size) {
		// Aligns the size to page boundaries
		// ceil(a / b) = floor((a + b - 1) / b)
		size = (size + getPageSize() - 1) / getPageSize() * getPageSize();
		CORE_ASSERT(size % getPageSize() == 0, "should be aligned to page size");

		int  flags = MAP_ANONYMOUS | MAP_PRIVATE;
		auto memory
			= reinterpret_cast<byte*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, flags, -1, 0));

		CORE_ASSERT_SYSCALL(memory != MAP_FAILED, "unable to allocate memory");
		return JitFuncMemory{ memory, size };
	}

	void JitFuncMemory::markExecutable() {
		CORE_ASSERT_SYSCALL(
			mprotect(addr, size, PROT_READ | PROT_EXEC) == 0, "unable to mark memory as executable"
		);
	}

	JitFuncMemory::~JitFuncMemory() noexcept {
		CORE_ASSERT_SYSCALL_NOEXCEPT(munmap(addr, size) == 0, "unable to unmap memory");
	}
}

#elif _WIN32
	#include <windows.h>

	#include <cstddef>
	#include <iostream>

namespace vm::jit::cnp {
	usize getPageSize() {
		static SYSTEM_INFO system_info = []() {
			SYSTEM_INFO system_info;
			GetSystemInfo(&system_info);
			return system_info;
		};
		return system_info.dwPageSize;
	}

	JitMemory JitMemory::allocate(usize size) {
		CORE_ASSERT(size % getPageSize() == 0, "should be aligned to page size");
		int   flags = MAP_ANONYMOUS | MAP_PRIVATE;
		auto* memory
			= reinterpret_cast<byte*>(VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE););

		return JitMemory{ .memory = memory, .size = size };
	}

	void JitMemory::mark_executable() {
		DWORD dummy;
		VirtualProtect(memory, size, PAGE_EXECUTE_READ, &dummy);
	}

	void JitMemory::free_jit_memory() { VirtualFree(memory, 0, MEM_RELEASE); }

}
#endif
