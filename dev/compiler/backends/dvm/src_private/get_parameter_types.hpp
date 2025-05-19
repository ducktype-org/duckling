#pragma once

#include <helios/scope_symbol_id.hpp>
#include <query_framework/context_fd.hpp>
#include <typesystem/lower/queries.hpp>
#include <typesystem/lower/type_layout.hpp>

namespace compiler::backend_vm {

	struct FunctionSignature {
		std::vector<tsl::TypeLayout> parameters;
		tsl::TypeLayout              result_type;
	};

	/**
	 * Helper function to get type layouts of parameters and return value from HELIOS Symbol ID.
	 * It is used to fetch function signatures for function outside of current module.
	 */
	FunctionSignature getParameterAndResultFromSymID(
		query::Context& ctx, helios::SymID helios_symbol
	);

}
