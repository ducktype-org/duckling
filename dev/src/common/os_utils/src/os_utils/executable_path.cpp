
#include "executable_path.hpp"

#include <base/misc/int_conv.hpp>

#include <array>
#include <string>

namespace os_utils {
	

#if defined(_WIN32)
	#include <windows.h>

	std::string getExecutablePathStr() {
		std::array<char, MAX_PATH> buffer{};

		DWORD len = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (len == 0) CORE_PANIC("Failed to get executable path on Windows");

		return std::string(buffer.data(), len);
	}

#elif defined(__APPLE__)
	#include <mach-o/dyld.h>

	std::string getExecutablePathStr() {
		std::array<char, 1'024> buffer{};
		uint32_t                size = static_cast<uint32_t>(buffer.size());

		if (_NSGetExecutablePath(buffer.data(), &size) != 0)
			CORE_PANIC("Failed to get executable path on macOS");

		return std::string(buffer.data());
	}

#else  // Linux
	#include <unistd.h>

	#include <climits>

	std::string getExecutablePathStr() {
		std::array<char, PATH_MAX> buffer{};

		ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
		if (len == -1) CORE_PANIC("Failed to get executable path on Linux");

		buffer.at(base::safeIntConv<usize>(len)) = '\0';
		return { buffer.data() };
	}

#endif

	fs::FilePath getExecutablePath() { return { getExecutablePathStr() }; }

}
