// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace compiler::frontend {

	/**
	 * This flag indicates whether remove functions in ModuleTreeModifier are enabled.
	 * Use of module modifier only make sense in language server mode.
	 * This flag enables additional checks for dangling references in dev mode.
	 */
	extern constinit bool use_module_modifier_remove;
}
