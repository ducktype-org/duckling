// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "executable_path.hpp"

#include <base/config/target_info.hpp>
#include <base/misc/int_conv.hpp>

#include <array>
#include <string>

// Platform includes; the body chain below mirrors this ladder.
#if BASE_TARGET_OS_WINDOWS
	#include <windows.h>
#elif BASE_TARGET_OS_MACOS
	#include <mach-o/dyld.h>
#elif BASE_TARGET_OS_LINUX
	#include <unistd.h>

	#include <climits>
#else
	#error "Unsupported target operating system."
#endif

namespace os_utils {

#if BASE_TARGET_OS_WINDOWS

	std::string getExecutablePathStr() {
		std::array<char, MAX_PATH> buffer{};

		DWORD len = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (len == 0) CORE_PANIC("Failed to get executable path on Windows");

		return std::string(buffer.data(), len);
	}

#elif BASE_TARGET_OS_MACOS

	std::string getExecutablePathStr() {
		std::array<char, 1'024> buffer{};
		uint32_t                size = static_cast<uint32_t>(buffer.size());

		if (_NSGetExecutablePath(buffer.data(), &size) != 0)
			CORE_PANIC("Failed to get executable path on macOS");

		return std::string(buffer.data());
	}

#elif BASE_TARGET_OS_LINUX

	std::string getExecutablePathStr() {
		std::array<char, PATH_MAX> buffer{};

		ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
		if (len == -1) CORE_PANIC("Failed to get executable path on Linux");

		buffer.at(base::safeIntConv<usize>(len)) = '\0';
		return { buffer.data() };
	}

#else
	#error "Unsupported target operating system."
#endif

	fs::FilePath getExecutablePath() { return { getExecutablePathStr() }; }

}
