#pragma once


#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	using QueryTypeFromDefinition_Result = query::QResult<tsh::SymbolType<>>;

	/**
	 * @brief Query tsh::AbstractTypeImpl from a symbol definition (like class definition).
	 *
	 * Example:
	 * class T {
	 *	...
	 * }
	 * - Then we can use this query QueryTypeFromDefinition(T).
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryTypeFromDefinition, SymID, CRef<QueryTypeFromDefinition_Result>, ({}));
}
