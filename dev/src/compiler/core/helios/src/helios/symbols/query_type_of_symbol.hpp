#pragma once

#include <helios/helios_errors.hpp>
#include <helios/scope_symbol_id.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <typesystem/higher/symbol_type.hpp>

namespace compiler::helios {

	using QuerySymbolType_Result
		= query::detail::errors::QResult<tsh::SymbolType<>, errors::Failed>;

	/**
	 * @brief Query type of the symbol.
	 */
	DECLARE_QUERY(QueryTypeOfSymbol, SymID, CRef<QuerySymbolType_Result>);

}
