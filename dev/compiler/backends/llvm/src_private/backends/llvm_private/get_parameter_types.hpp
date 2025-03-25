#pragma once

#include <query_framework/query_impl.hpp> // @TODO #404 relax it to just context in the header

#include <typesystem/lower/type_layout.hpp>
#include <typesystem/lower/queries.hpp>
#include <helios/symbols/symbols.hpp>

namespace compiler::backend_llvm {

    // Move to lir / mir?
    struct ParametersAndReturn {
        std::vector<tsl::TypeLayout> parameters;
        tsl::TypeLayout result_type;
    };

	/**
	 * Helper function to get parameter types from HELIOS Symbol ID.
	 * ... TODO this PR: not inline
	 */
	inline ParametersAndReturn getParameterAndResultFromSymID(query::Context& ctx, helios::SymID helios_symbol) {
		auto type = ctx.query<helios::QueryTypeOfSymbol>(helios_symbol)->value();
        auto function_type = tsh::FunctionAbstractType(type.getType());
        const auto& tsh_parameters = function_type.getParameterTypes();

        std::vector<tsl::TypeLayout> parameters;
        parameters.reserve(tsh_parameters.size());
        for (const auto& param: tsh_parameters) {
            parameters.push_back(ctx.query<tsl::QuerySymbolTypeLayout>(param));
        }

        auto result_type = ctx.query<tsl::QuerySymbolTypeLayout>(function_type.getResultType());
        return {.parameters = parameters, .result_type = result_type};
	}
}


