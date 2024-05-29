/**
 * @file hout.hpp
 * @note This file is a placeholder for HOUT structures
 * that will be added in the future.
 */

#pragma once

#include "elements.hpp"

// @TODO: relax this dependency
#include <frontend/module_tree/module_tree.hpp>  // ModuleId

#include <vector>


namespace compiler::helios {

	/**
	 * @brief placeholder for code that can execute (expressions, function body, etc)
	 */
	struct HOUTCode {
		// @NOTE: as of right now HOUTCode structure can be based on shared ptr, to avoid a lot of boilerplate, and copying 


		std::shared_ptr<const code::CodeBlock> body;
	};

	/**
	 * @brief placeholder for functions, methods, etc
	 */
	struct HOUTFunction {
		// @TODO:
		// - args
		// - rets
		// - some other stuff from proposal

		base::StrId original_name;
		
		HOUTCode body;

		[[nodiscard]]
		std::string debugPrint() const;
	};

	/**
	 * @brief Represents a constant
	 * @note: this is a mock
	 * @TODO: make it represent more general stuff
	 */
	struct HOUTGlobalData {
		// @TODO: types
		
		// @TODO: should it be here -- it is now for pretty printing, but it is not so clear?
		SymID helios_symbol;
		
		base::StrId original_name;

		// @TODO: CTV from TS:
		i64 value;

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

		std::vector<HOUTGlobalData> glob_data;

		std::vector<HOUTFunction> functions;


		[[nodiscard]]
		std::string debugPrint() const;
	};

}
