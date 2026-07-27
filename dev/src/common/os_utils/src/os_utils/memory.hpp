#pragma once

#include <base/types/ints.hpp>

#include <expected>
#include <string>

namespace os_utils {
	std::expected<usize, std::string> getPageSize();
	std::expected<byte*, std::string> allocatePages(usize size);
	std::expected<void, std::string>  markExecutable(byte* addr, usize size);
	void                              freePages(byte* addr, usize size);
}
