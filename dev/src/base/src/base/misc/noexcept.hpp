// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/config/build_type.hpp>

/**
 * @brief Function will not throw in Release build
 */
#define RELEASE_NOEXCEPT noexcept(::base::IS_BUILD_TYPE_RELEASE)
