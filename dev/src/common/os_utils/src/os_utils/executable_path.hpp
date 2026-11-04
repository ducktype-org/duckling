// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <filesystem/file_path.hpp>

namespace os_utils {
	/**
	 * @brief Helper that returns the path to the executable that is currently running.
	 * Works on all platforms.
	 */
	fs::FilePath getExecutablePath();
}
