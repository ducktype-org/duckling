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
}
