#include "memory.hpp"

#include <base/except/exceptions.hpp>

#include <os_utils/memory.hpp>

#include <expected>
#include <fstream>
#include <utility>

namespace vm::jit::cnp {
	void JitFuncMemory::dump(const char* filename) {
		std::ofstream file{ filename, std::ios::binary };

		for (byte b: span()) file << std::to_underlying(b);
	}

	std::expected<JitFuncMemory, std::string> JitFuncMemory::allocate(usize size) {
		return os_utils::getPageSize().and_then(
			[&](usize page_size) -> std::expected<JitFuncMemory, std::string> {
				// Aligns the size to page boundaries
			    // ceil(a / b) = floor((a + b - 1) / b)
				size = (size + page_size - 1) / page_size * page_size;
				CORE_ASSERT(size % page_size == 0, "should be aligned to page size");

				return os_utils::allocatePages(size).and_then(
					[&](byte* memory) -> std::expected<JitFuncMemory, std::string> {
						return JitFuncMemory{ memory, size };
					}
				);
			}
		);
	}

	std::expected<void, std::string> JitFuncMemory::markExecutable() {
		return os_utils::markExecutable(addr, size);
	}

	JitFuncMemory::~JitFuncMemory() noexcept {
		if (addr != nullptr) os_utils::freePages(addr, size);
	}
}
