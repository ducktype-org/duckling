// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace compiler::driver {
	/**
	 * @brief Save compilation artifacts to disk: serializes the query graph and metadata, reclaims
	 * on-disk caches orphaned by this compilation, and flushes everything to disk.
	 */
	void saveArtifacts();
}
