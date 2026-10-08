// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "system_libraries.hpp"

#include <base/config/target_info.hpp>

namespace os_utils {
#if BASE_TARGET_OS_MACOS
	const std::string& systemSharedLibC() {
		static const std::string library = "libSystem.B.dylib";
		return library;
	}

	const std::string& systemSharedLibM() { return systemSharedLibC(); }
#elif BASE_TARGET_OS_WINDOWS
	#error "systemSharedLibC/systemSharedLibM are not supported on Windows"
#else
	const std::string& systemSharedLibC() {
		static const std::string library = "libc.so.6";
		return library;
	}

	const std::string& systemSharedLibM() {
		static const std::string library = "libm.so.6";
		return library;
	}
#endif
}
