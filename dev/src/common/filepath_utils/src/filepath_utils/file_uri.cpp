// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "file_uri.hpp"

#include <base/config/target_info.hpp>

namespace filepath_utils {

	std::string formatFileUri(std::string_view path) {
		if constexpr (base::IS_TARGET_OS_WINDOWS)
			if (path.size() >= 2 && path[1] == ':') return "file:///" + std::string(path);
		return "file://" + std::string(path);
	}

}
