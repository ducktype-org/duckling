#pragma once

namespace compiler::frontend {

	/**
	 * This flag indicates whether remove functions in ModuleTreeModifier are enabled.
	 * Use of module modifier only make sense in language server mode.
	 * This flag enables additional checks for dangling references in dev mode.
	 */
	extern constinit bool use_module_modifier_remove;
}
