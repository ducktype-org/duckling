#include "get_parameter_types.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <query_framework/context.hpp>

namespace compiler::backend_vm {

	ParametersAndReturn getParameterAndResultFromSymID(
		query::Context& ctx, helios::SymID helios_symbol
	) {
		auto        type           = ctx.query<helios::QueryTypeOfSymbol>(helios_symbol)->value();
		auto        function_type  = tsh::FunctionAbstractType(type.getType());
		const auto& tsh_parameters = function_type.getParameterTypes();

		std::vector<tsl::TypeLayout> parameters;
		parameters.reserve(tsh_parameters.size());
		for (const auto& param: tsh_parameters)
			parameters.push_back(ctx.query<tsl::QuerySymbolTypeLayout>(param));

		auto result_type = ctx.query<tsl::QuerySymbolTypeLayout>(function_type.getResultType());
		return { .parameters = parameters, .result_type = result_type };
	}
}
