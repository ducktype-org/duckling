// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/** @file
 * @brief Stub build ID header used by IDEs and linters.
 *
 * The real build ID is generated into the build directory at build time and
 * takes precedence via the include path order. This source-tree stub exists so
 * tools that inspect source files can resolve <artifacts/build_id.hpp> without
 * needing the generated file.
 */

#pragma once

#include <string_view>

namespace artifacts {
	inline constexpr std::string_view BUILD_ID = "";
}
