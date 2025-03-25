#pragma once

#include <query_framework/query_impl.hpp> // @TODO #404 relax it to just context

#include <typesystem/lower/type_layout.hpp>
#include <typesystem/lower/queries.hpp>
#include <helios/symbols/symbols.hpp>

namespace compiler::backend_llvm {


	/**
	 * Helper function to get parameter types from symbol ID.
	 * ... TODO this PR: not inline
	 */
	inline std::vector<tsl::TypeLayout> getParameterTypeFromSymID(query::Context& ctx, helios::SymID helios_symbol) {
		auto type = ctx.query<helios::QueryTypeOfSymbol>(helios_symbol)->value();
        auto function_type = tsh::FunctionAbstractType(type.getType());
        const auto& parameters = function_type.getParameterTypes();

        std::vector<tsl::TypeLayout> result;
        result.reserve(parameters.size());
        for (const auto& param: parameters) {
            result.push_back(ctx.query<tsl::QuerySymbolTypeLayout>(param));
        }
        return result;
	}
}


