#pragma once

#include <base/stable_container.hpp>
#include "forward.hpp"

namespace compiler::helios {

	/**
	 * @brief Reference to HELIOS-symbol
	 */
	using SymbolRef = base::borrow_ptr<SymbolData>;
	using ScopeRef = base::borrow_ptr<ScopeData>;

	struct SymID {
	private:
		SymbolRef ref;
	};
	struct ScopeID {
	private:
		ScopeRef ref;
	};

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

