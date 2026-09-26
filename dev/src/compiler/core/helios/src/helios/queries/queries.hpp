/**
 * @file queries.hpp
 * @brief This file contains top-level queries for interacting with HELIOS.
 * @note Other queries related to Symbols and Scopes can albo be called from outside HELIOS.
 */
#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	// @FUTURE: perhaps we will need to add more granularity to HOUT generation for efficient
	// incremental compilation

	/**
	 * @brief Query FULL HOUTUnit of single module
	 *
	 * \parallel key helpers like isGlobalVar don’t modify globals
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryModuleHOUT,
		frontend::ModuleID,
		CRef<query::QResult<HOUTUnit>>,
		({
			// Compile module schedules other queries, so we don't want to interrupt it in the
	        // middle of execution.
	        // @TODO: #2496 maybe remove this tag.
			.catch_exceptions_if_using_qresult = false,
		})
	)

	/**
	 * @brief Query HOUTUnit of module and all its submodules recursively
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryModuleHOUTRecursively,
		frontend::ModuleID,
		query::QResult<std::vector<CRef<HOUTUnit>>>,
		({})
	)

	/**
	 * @brief Debug/testing query for extracting top-level functions and constants from module
	 *
	 * \parallel key helpers like isGlobalVar don’t modify globals
	 * \query_thread_safe_if_cache
	 * @TODO: #2878 remove this maybe -- try to replace its usages with QueryModuleHOUT
	 */
	DECLARE_QUERY(QueryTopLevelEntities, frontend::ModuleID, CRef<query::QResult<HOUTUnit>>, ({}))

	// =================================== Utilities ===================================
	base::OkBad collectReplicatedSymbols(query::Context& ctx, HOUTUnit& out_unit);
}
