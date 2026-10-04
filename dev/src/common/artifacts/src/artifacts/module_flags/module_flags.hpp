// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace artifacts {
	/**
	 * @brief If true, the root ArtifactCollection validates the stored build id
	 * against the current compiler's BUILD_ID and wipes the directory on mismatch.
	 *
	 * Compile-time constant - flip locally when debugging artifact formats across
	 * compiler builds.
	 */
	constexpr bool CHECK_BUILD_ID = true;
}
