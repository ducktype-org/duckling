// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <optional>
#include <string_view>

namespace dia::templates {
	std::optional<std::string_view> loadTemplateFromPath(std::string_view path);
}
