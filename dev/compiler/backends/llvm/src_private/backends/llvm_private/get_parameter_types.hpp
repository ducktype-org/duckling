#pragma once

#include <helios/symbols/symbols.hpp>
#include <query_framework/context_fd.hpp>
#include <typesystem/lower/queries.hpp>
#include <typesystem/lower/type_layout.hpp>

namespace compiler::backend_llvm {

	// Move to lir / mir?
	struct ParametersAndReturn {
		std::vector<tsl::TypeLayout> parameters;
		tsl::TypeLayout              result_type;
	};

	/**
	 * Helper function to get parameter types from HELIOS Symbol ID.
	 */
	ParametersAndReturn
		getParameterAndResultFromSymID(query::Context& ctx, helios::SymID helios_symbol);

}
