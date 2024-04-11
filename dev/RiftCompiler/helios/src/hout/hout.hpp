#pragma once

#include "../scope_symbol_id.hpp"

namespace compiler::helios {

	// placeholder for functions, methods, etc
	struct HOUTFunction { };

	/**
	 * @brief Structure representing HOUT of single module
	 */
	struct HOUTModule {
		// all first class citizens of module should be here:
		// * types (in some way?)
		// * required baked template list?
		// * functions / methods / macros
		// * defined templates
		// * vector/references to hout of submodules? -- not necessarily needed
		// * what else?
	};

}

// @TODO std hash..

