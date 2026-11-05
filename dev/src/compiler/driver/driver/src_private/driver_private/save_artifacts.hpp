// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <artifacts/artifacts.hpp>

namespace compiler::driver {
	/**
	 * @brief Save compilation artifacts to disk: serializes the query graph and metadata, removes
	 * query artifacts no build has used for a while and stale temporary files, and flushes
	 * everything to disk.
	 */
	void saveArtifacts();

	/**
	 * @brief Removes the temporary files that killed writes left in `collection`'s tree and that
	 * are too old to belong to a write still in progress.
	 */
	void removeStaleTemporaryFiles(Ref<artifacts::ArtifactCollection> collection);
}
