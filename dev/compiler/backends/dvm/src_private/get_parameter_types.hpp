#pragma once

#include <helios/scope_symbol_id.hpp>
#include <query_framework/context_fd.hpp>
#include <typesystem/lower/queries.hpp>
#include <typesystem/lower/type_layout.hpp>

namespace compiler::backend_vm {

	struct ParametersAndReturn {
		std::vector<tsl::TypeLayout> parameters;
		tsl::TypeLayout              result_type;
	};

	/**
	 * Helper function to get type layouts of parameters and return value from HELIOS Symbol ID.
	 */
	ParametersAndReturn getParameterAndResultFromSymID(
		query::Context& ctx, helios::SymID helios_symbol
	);

}
