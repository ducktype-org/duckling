// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <artifacts/artifacts.hpp>

namespace compiler::driver {

	/**
	 * @brief Get the artifact collection where the compiler should save its own
	 * debug artifacts,such as IR dumps. Note that it is not for the debug info of the
	 * compiled code, but rather about the insights of the compiler itself.
	 *
	 * @return Ref<artifacts::ArtifactCollection>
	 */
	Ref<artifacts::ArtifactCollection> getDebugArtifactCollection();
}
