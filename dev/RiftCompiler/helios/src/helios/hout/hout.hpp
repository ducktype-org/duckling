#pragma once

#include "../scope_symbol_id.hpp"
#include <vector>

namespace compiler::helios {


	struct HOUTCode {};

	// placeholder for functions, methods, etc
	struct HOUTFunction {};

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

		// @TODO: do we need separation:
		std::vector<SymID> first_class_citizens;
	};

}

// @TODO std hash..
