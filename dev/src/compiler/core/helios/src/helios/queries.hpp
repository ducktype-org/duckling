/**
 * @file queries.hpp
 * @brief This file contains top-level queries for interacting with HELIOS.
 * @note Other queries related to Symbols and Scopes can albo be called from outside HELIOS.
 */
#pragma once


#include "hout/hout_fd.hpp"
#include "scope_symbol_id.hpp"

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	// @FUTURE: perhaps we will need to add more granularity to HOUT generation for efficient
	// incremental compilation

	/**
	 * @brief Query FULL HOUTUnit of single module
	 */
	DECLARE_QUERY(QueryModuleHOUT, frontend::ModuleID, query::QResult<HOUTUnit>, ({}))

	/**
	 * @brief Query HOUTUnit of module and all its submodules recursively
	 */
	DECLARE_QUERY(
		QueryModuleHOUTRecursively, frontend::ModuleID, query::QResult<std::vector<HOUTUnit>>, ({})
	)

	/**
	 * @brief Debug/testing query for extracting top-level functions and constants from module
	 */
	DECLARE_QUERY(QueryTopLevelEntities, frontend::ModuleID, CRef<query::QResult<HOUTUnit>>, ({}))

	/**
	 * @brief Query REPL statement/module to HOUTUnit. For REPL sessions we treat the
	 * provided in-memory module/sourcefile similarly to a module and produce HOUTUnit.
	 */
	DECLARE_QUERY(QueryReplStatementTo, frontend::ModuleID, HOUTUnit)

	/**
	 * @brief Query declaration of function: types, args and its names.
	 * @note Unlike QueryCodeOfFun, this query works for all SymID-s that represent functions,
	 * be it user-defined, extern, built-in, or generated.
	 */
	DECLARE_QUERY(QueryDeclOfFun, SymID, CRef<query::QResult<HOUTFunctionDeclaration>>, ({}));

	/**
	 * @brief Query code of a function.
	 * @note Works only for SymID-s that actually represent PST-function (i.e. PST symbol).
	 */
	DECLARE_QUERY(QueryCodeOfFun, SymID, query::QResult<HOUTFunction>, ({}));
}
