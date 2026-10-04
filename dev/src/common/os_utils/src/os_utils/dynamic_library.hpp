#pragma once

#include <base/types/ints.hpp>

#include <expected>
#include <span>
#include <string>

namespace os_utils {

	/**
	 * @brief Opaque handle to a native shared library.
	 *
	 * Wraps the dlopen handle returned by openLibrary() or
	 * openLibraryFromMemory(). Both functions release their backing
	 * descriptor (memfd / temp file) before returning; the loaded image
	 * stays valid without it.
	 *
	 * A default-constructed NativeLibrary is a valid empty handle;
	 * closeLibrary() on it is a no-op. Resources are released by
	 * closeLibrary().
	 */
	struct NativeLibrary {
		void* handle = nullptr;
	};

	/**
	 * @brief Loads a shared library from a file path.
	 *
	 * @param path Library path, or nullptr to open the main program itself.
	 * @return A library handle, or an error message.
	 */
	std::expected<NativeLibrary, std::string> openLibrary(const char* path);

	/**
	 * @brief Loads a shared library from in-memory bytes.
	 *
	 * The bytes are staged into a memfd (Linux) or a temp file (macOS) and
	 * loaded from there.
	 *
	 * @param library_bytes The complete contents of a shared library file.
	 * @return A library handle, or an error message.
	 */
	std::expected<NativeLibrary, std::string> openLibraryFromMemory(
		std::span<const byte> library_bytes
	);

	/**
	 * @brief Looks up a symbol in a loaded library.
	 *
	 * @param lib Library handle from openLibrary() or openLibraryFromMemory().
	 * @param name Symbol name.
	 * @return The symbol address, or an error message.
	 */
	std::expected<void*, std::string> findSymbol(const NativeLibrary& lib, const char* name);

	/**
	 * @brief Closes a library and releases its resources.
	 *
	 * Resets the handle to its default state; calling twice is safe.
	 */
	void closeLibrary(NativeLibrary& lib);
}
