#include "standard_library.hpp"

#include <base/misc/int_conv.hpp>

#include <array>
#include <string>

#if defined(_WIN32)
	#include <windows.h>

std::string getExecutablePathStr() {
	std::array<char, MAX_PATH> buffer{};

	DWORD len = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
	if (len == 0) return "";

	return std::string(buffer.data(), len);
}

#elif defined(__APPLE__)
	#include <mach-o/dyld.h>

std::string getExecutablePathStr() {
	std::array<char, 1'024> buffer{};
	uint32_t                size = static_cast<uint32_t>(buffer.size());

	if (_NSGetExecutablePath(buffer.data(), &size) != 0)
		return "";  // buffer too small (simplified handling)

	return std::string(buffer.data());
}

#else  // Linux
	#include <limits.h>
	#include <unistd.h>

std::string getExecutablePathStr() {
	std::array<char, PATH_MAX> buffer{};

	ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
	if (len == -1) return "";

	buffer.at(base::safeIntConv<usize>(len)) = '\0';
	return { buffer.data() };
}

#endif

fs::FilePath getExecutablePath() { return { getExecutablePathStr() }; }
