#pragma once

#include <string>

namespace os_utils {
	/**
	 * @brief The soname of the system C library, as the platform's dynamic loader resolves it.
	 * @note macOS has no standalone C library - its symbols live in the umbrella library, so this
	 * and `systemSharedLibM` name the same file there.
	 */
	const std::string& systemSharedLibC();

	/**
	 * @brief The soname of the system math library, as the platform's dynamic loader resolves it.
	 * @note See the note on `systemSharedLibC` about macOS.
	 */
	const std::string& systemSharedLibM();
}
