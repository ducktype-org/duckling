#pragma once

#include <base/types/ints.hpp>

#include <expected>
#include <span>
#include <string>

namespace os_utils {

	/**
	 * @brief Opaque handle to a native shared library.
	 */
	struct NativeLibrary {
		void* handle = nullptr;
		int   fd     = -1;  // file descriptor, -1 when not applicable
	};

	/// dlopen a library from a file path.
	std::expected<NativeLibrary, std::string> openLibrary(const char* path);

	/// Load a library from in-memory bytes (staged via memfd or temp file).
	std::expected<NativeLibrary, std::string> openLibraryFromMemory(
		std::span<const byte> library_bytes
	);

	/// dlsym a symbol. Returns the symbol or an error message (from dlerror).
	std::expected<void*, std::string> findSymbol(const NativeLibrary& lib, const char* name);

	/// dlclose + close(fd).
	void closeLibrary(const NativeLibrary& lib);
}
