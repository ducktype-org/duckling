// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <global_state/artifacts_location.hpp>

namespace compiler::driver {
	/**
	 * Exit the driver: persist artifacts and any driver-managed data (e.g. query graph),
	 * then flush to disk.
	 */
	void exit();
}
