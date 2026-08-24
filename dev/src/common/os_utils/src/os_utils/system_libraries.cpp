#include "system_libraries.hpp"

namespace os_utils {
#ifdef __APPLE__
	const std::string& systemSharedLibC() {
		static const std::string library = "libSystem.B.dylib";
		return library;
	}

	const std::string& systemSharedLibM() { return systemSharedLibC(); }
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
