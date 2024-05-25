/**
 * @file hout.hpp
 * @note This file is a placeholder for HOUT structures
 * that will be added in the future.
 */

#pragma once

#include "../scope_symbol_id.hpp"
#include "element_ref.hpp"

// @TODO: relax this dependency
#include <frontend/module_tree/module_tree.hpp>  // ModuleId

#include <vector>


namespace compiler::helios {

	namespace code {
		struct CodeBlock {
			// @TODO
		};

		struct Expr {
			// @TODO
		};
	}

	/**
	 * @brief placeholder for code that can execute (expressions, function body, etc)
	 */
	struct HOUTCode {
		code::ElementRef<code::CodeBlock> body;
	};

	/**
	 * @brief placeholder for functions, methods, etc
	 */
	struct HOUTFunction {
		// @TODO:
		// - args
		// - rets
		// - some other stuff from proposal

		// @TODO:
		HOUTFunction(const HOUTFunction&);

		HOUTCode body;

		[[nodiscard]]
		std::string debugPrint() const;
	};

	/**
	 * @brief Structure representing single HOUTUnit
	 */
	struct HOUTUnit {
		// all first class citizens of module should be here:
		// * types (in some way?)
		// * required baked template list?
		// * functions / methods / macros
		// * defined templates
		// * vector/references to hout of submodules? -- not necessarily needed
		// * what else?

		std::vector<HOUTFunction> functions;


		[[nodiscard]]
		std::string debugPrint() const;
	};

}
