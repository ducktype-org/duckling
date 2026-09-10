#include "system_libraries.hpp"

#include <base/config/target_info.hpp>

namespace os_utils {
#if BASE_TARGET_OS_MACOS
	const std::string& systemSharedLibC() {
		static const std::string LIBRARY = "libSystem.B.dylib";
		return LIBRARY;
	}

	const std::string& systemSharedLibM() { return systemSharedLibC(); }
#elif BASE_TARGET_OS_WINDOWS
	#error "systemSharedLibC/systemSharedLibM are not supported on Windows"
#else
	const std::string& systemSharedLibC() {
		static const std::string LIBRARY = "libc.so.6";
		return LIBRARY;
	}

	const std::string& systemSharedLibM() {
		static const std::string LIBRARY = "libm.so.6";
		return LIBRARY;
	}
#endif
}
