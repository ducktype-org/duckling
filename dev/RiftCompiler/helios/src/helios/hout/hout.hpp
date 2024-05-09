/**
 * @file hout.hpp
 * @note This file is a placeholder for HOUT structures
 * that will be added in the future. 
 */

#pragma once

#include "../scope_symbol_id.hpp"

// @TODO: relax this dependency
#include <frontend/module_tree/module_tree.hpp>  // ModuleId

#include <vector>

namespace compiler::helios {

	/**
	 * @brief placeholder for code that can execute (expressions, function body, etc)
	 */
	struct HOUTCode {};

	/**
	 * @brief placeholder for functions, methods, etc
	 */
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

		// @FUTURE: we will probably need separation:
		std::vector<SymID> first_class_citizens;

		std::vector<frontend::ModuleId> imported_modules;
	};

}
