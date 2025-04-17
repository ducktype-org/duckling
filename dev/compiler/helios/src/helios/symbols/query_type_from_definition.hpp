#pragma once

#include <typesystem/higher/symbol_type.hpp>
#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>
#include <query_framework/query_int.hpp>
#include <helios/scope_symbol_id.hpp>

namespace compiler::helios {
    
	using QueryTypeFromDefinition_Result = errors::HResult<tsh::SymbolType<>, errors::Failed>;

	/**
	 * @brief Query tsh::AbstractTypeImpl from a symbol definition (like class definition).
	 *
	 * Example:
	 * class T {
	 *	...
	 * }
	 * - Then we can use this query QueryTypeFromDefinition(T).
	 */
	DECLARE_QUERY(QueryTypeFromDefinition, SymID, CRef<QueryTypeFromDefinition_Result>);
}