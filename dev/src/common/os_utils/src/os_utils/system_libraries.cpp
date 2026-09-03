#include "system_libraries.hpp"

namespace os_utils {
#ifdef __APPLE__
	const std::string& systemSharedLibC() {
		static constexpr std::string LIBRARY = "libSystem.B.dylib";
		return LIBRARY;
	}

	const std::string& systemSharedLibM() { return systemSharedLibC(); }
#elif defined(_WIN32)
	#error "systemSharedLibC/systemSharedLibM are not supported on Windows"
#else
	const std::string& systemSharedLibC() {
		static constexpr std::string LIBRARY = "libc.so.6";
		return LIBRARY;
	}

	const std::string& systemSharedLibM() {
		static constexpr std::string LIBRARY = "libm.so.6";
		return LIBRARY;
	}
#endif
}
