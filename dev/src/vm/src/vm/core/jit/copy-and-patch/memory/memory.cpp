#include "memory.hpp"

#include <expected>

#if __unix__
	#include <sys/mman.h>
	#include <unistd.h>

	#include <base/except/exceptions.hpp>

namespace vm::jit::cnp {
	namespace {
		std::expected<usize, std::string> getPageSize() {
			static auto page_size = []() -> std::expected<usize, std::string> {
				long result = sysconf(_SC_PAGESIZE);
				if (result == -1) return std::unexpected<std::string>("couldn't get the page size");
				return static_cast<usize>(result);
			}();
			return page_size;
		}
	}

	std::expected<JitFuncMemory, std::string> JitFuncMemory::allocate(usize size) {
		return getPageSize().and_then(
			[&](usize page_size) -> std::expected<JitFuncMemory, std::string> {
				// Aligns the size to page boundaries
			    // ceil(a / b) = floor((a + b - 1) / b)
				size = (size + page_size - 1) / page_size * page_size;
				CORE_ASSERT(size % page_size == 0, "should be aligned to page size");

				int  flags  = MAP_ANONYMOUS | MAP_PRIVATE;
				auto memory = reinterpret_cast<byte*>(
					mmap(nullptr, size, PROT_READ | PROT_WRITE, flags, -1, 0)
				);

				if (memory == MAP_FAILED)
					return std::unexpected<std::string>("unable to allocate memory");

				return JitFuncMemory{ memory, size };
			}
		);
	}

	std::expected<void, std::string> JitFuncMemory::markExecutable() {
		if (mprotect(addr, size, PROT_READ | PROT_EXEC) != 0)
			return std::unexpected<std::string>("unable to mark memory as executable");
		return {};
	}

	JitFuncMemory::JitFuncMemory(JitFuncMemory&& other) noexcept:
		  addr{ other.addr },
		  size{ other.size } {
		other.addr = nullptr;
		other.size = 0;
	}

	JitFuncMemory& JitFuncMemory::operator=(JitFuncMemory&& other) noexcept {
		if (this != &other) {
			if (addr != nullptr)
				CORE_ASSERT_NOEXCEPT(munmap(addr, size) == 0, "unable to unmap memory");
			addr       = other.addr;
			size       = other.size;
			other.addr = nullptr;
			other.size = 0;
		}
		return *this;
	}

	JitFuncMemory::~JitFuncMemory() noexcept {
		if (addr != nullptr)
			CORE_ASSERT_NOEXCEPT(munmap(addr, size) == 0, "unable to unmap memory");
	}
}

#else
	#error "Unsupported system"
#endif
