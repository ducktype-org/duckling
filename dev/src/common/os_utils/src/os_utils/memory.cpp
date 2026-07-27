#include "memory.hpp"

#if __unix__
	#include <sys/mman.h>
	#include <unistd.h>
#else
	#error "os_utils::memory: unsupported platform"
#endif

namespace os_utils {

	std::expected<usize, std::string> getPageSize() {
		static auto page_size = []() -> std::expected<usize, std::string> {
			long result = sysconf(_SC_PAGESIZE);
			if (result == -1) return std::unexpected<std::string>("couldn't get the page size");
			return static_cast<usize>(result);
		}();
		return page_size;
	}

	std::expected<byte*, std::string> allocatePages(usize size) {
		int  flags = MAP_ANONYMOUS | MAP_PRIVATE;
		auto memory
			= reinterpret_cast<byte*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, flags, -1, 0));

		if (memory == MAP_FAILED) return std::unexpected<std::string>("unable to allocate memory");

		return memory;
	}

	std::expected<void, std::string> markExecutable(byte* addr, usize size) {
		if (mprotect(addr, size, PROT_READ | PROT_EXEC) != 0)
			return std::unexpected<std::string>("unable to mark memory as executable");
		return {};
	}

	void freePages(byte* addr, usize size) {
		if (addr != nullptr) munmap(addr, size);
	}
}
