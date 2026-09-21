#include "memory.hpp"

#include <base/config/target_info.hpp>
#include <base/except/exceptions.hpp>

#if BASE_TARGET_PLATFORM_POSIX
	#include <sys/mman.h>
	#include <unistd.h>

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
		if (addr == nullptr) return;
		int res = munmap(addr, size);
		CORE_ASSERT_NOEXCEPT(res == 0, "unable to unmap memory");
	}
}

#else
namespace os_utils {
	// Unsupported platforms compile but return errors at runtime.
	// @TODO: #3343 Add Windows CI coverage for os_utils and filepath_utils platform branches.
	std::expected<usize, std::string> getPageSize() {
		return std::unexpected<std::string>("os_utils::memory: not implemented on this platform");
	}

	std::expected<byte*, std::string> allocatePages(usize) {
		return std::unexpected<std::string>("os_utils::memory: not implemented on this platform");
	}

	std::expected<void, std::string> markExecutable(byte*, usize) {
		return std::unexpected<std::string>("os_utils::memory: not implemented on this platform");
	}

	void freePages(byte*, usize) {}
}
#endif
