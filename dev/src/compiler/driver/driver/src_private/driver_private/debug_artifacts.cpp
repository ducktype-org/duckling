// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "debug_artifacts.hpp"

#include <global_state/artifacts_location.hpp>

namespace compiler::driver {
	Ref<artifacts::ArtifactCollection> getDebugArtifactCollection() {
		return global_state::getRootCollection()->subCollectionAtOrNew(
			base::StrID("duck_debug_artifacts")
		);
	}
}
