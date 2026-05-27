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
	 * @brief Key for QueryModuleHOUT.
	 *
	 * The bool controls whether simple-type toString helpers are included in the resulting HOUT.
	 * It defaults to true so existing callers keep the full module behavior.
	 *
	 * @TODO: #2833 remove the bool once proper deduplication in LIR merge is implemented
	 */
	struct QueryModuleHOUT_Key final {
		frontend::ModuleID module_id;
		bool               include_simple_type_helpers = true;

		QueryModuleHOUT_Key(frontend::ModuleID module_id, bool include_simple_type_helpers = true):
			  module_id(module_id),
			  include_simple_type_helpers(include_simple_type_helpers) {}

		[[nodiscard]] base::Bit256 queryUnstablePerfectHash() const {
			return { module_id.queryUnstablePerfectHash(),
				     static_cast<u64>(include_simple_type_helpers) };
		}

		bool operator==(const QueryModuleHOUT_Key&) const = default;
	};

	/**
	 * @brief Query FULL HOUTUnit of single module
	 *
	 * \parallel key helpers like isGlobalVar don’t modify globals
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryModuleHOUT,
		QueryModuleHOUT_Key,
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
	 * @TODO: #2246 remove this maybe -- try to replace its usages with QueryModuleHOUT
	 */
	DECLARE_QUERY(QueryTopLevelEntities, frontend::ModuleID, CRef<query::QResult<HOUTUnit>>, ({}))
}
