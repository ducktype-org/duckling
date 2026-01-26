#pragma once


#include <helios/symbol_id.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	using QuerySymbolType_Result = query::QResult<tsh::SymbolType<>>;

	/**
	 * @brief Query type of the symbol.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryTypeOfSymbol, SymID, CRef<QuerySymbolType_Result>, ({}))
}
