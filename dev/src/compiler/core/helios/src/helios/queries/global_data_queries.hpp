/**
 * @file global_data_queries.hpp
 * @brief Queries related to global data HOUT materialization.
 */
#pragma once

#include <helios/hout/hout.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	/**
	 * @brief Query HOUT representation of a global constant or variable.
	 *
	 * Expects key to point to a global-level symbol (const or variable).
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryHOUTGlobalData, SymID, CRef<query::QResult<HOUTGlobalData>>, ({}));

	/**
	 * @brief Returns the expression that should be used by the MIR global variable
	 * constructor - it in-place initializes the global.
	 */
	Box<code::Expr> getGlobalConstructorExpr(query::Context& ctx, CRef<HOUTGlobalData> global_data);

	/**
	 * @brief Returns the expression that should be used by the MIR global variable
	 * destructor - it takes the reference to the global and calls destructor function.
	 */
	base::Optional<Box<code::Expr>> getGlobalDestructorExpr(
		query::Context& ctx, CRef<HOUTGlobalData> global_data
	);
}
