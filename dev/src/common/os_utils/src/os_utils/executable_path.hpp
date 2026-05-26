#pragma once

#include <filesystem/file_path.hpp>

namespace os_utils {
	/**
	 * @brief Helper that returns the path to the executable that is currently running.
	 * Works on all platforms.
	 */
	fs::FilePath getExecutablePath();
}