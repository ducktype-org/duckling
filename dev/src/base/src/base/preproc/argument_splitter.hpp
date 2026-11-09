// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <string>
#include <vector>

namespace base {
	/**
	 * @brief Divides a list of arguments divided by commas into separate strings while ignoring any
	 * white spaces.
	 *
	 * @note This solution is somewhat over engineered.
	 */
	std::vector<std::string> vaArgSplit(std::string_view va_arg);
}
