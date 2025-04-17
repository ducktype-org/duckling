#pragma once

#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>
#include <helios/scope_symbol_id.hpp>
#include <query_framework/query_int.hpp>
#include <typesystem/higher/symbol_type.hpp>

namespace compiler::helios {

	using QuerySymbolType_Result = errors::HResult<tsh::SymbolType<>, errors::Failed>;

	/**
	 * @brief Query type of the symbol.
	 */
	DECLARE_QUERY(QueryTypeOfSymbol, SymID, CRef<QuerySymbolType_Result>);

}

