/**
 * @file queries.hpp
 * @brief This file contains top-level queries for interacting with HELIOS.
 * @note Other queries related to Symbols and Scopes can albo be called from outside HELIOS.
 */
#pragma once


#include "hout/hout_fd.hpp"
#include "scope_symbol_id.hpp"

#include <frontend/module_tree/module_id.hpp>

#include <query_framework/query_int.hpp>

namespace compiler::helios {
	// @FUTURE: perhaps we will need to add more granularity to HOUT generation for efficient
	// incremental compilation

	/**
	 * @brief Query FULL HOUTUnit of single module
	 */
	DECLARE_QUERY(QueryModuleHOUT, frontend::ModuleID, HOUTUnit)

	/**
	 * @brief Query HOUTUnit of module and all its submodules recursively
	 */
	DECLARE_QUERY(QueryModuleHOUTRecursively, frontend::ModuleID, std::vector<HOUTUnit>)

	/**
	 * @brief Debug/testing query for extracting top-level functions and constants from module
	 */
	DECLARE_QUERY(QueryTopLevelEntities, frontend::ModuleID, CRef<HOUTUnit>)

	/**
	 * @brief Query declaration of function: types, args and its names.
	 * @note Unlike QueryCodeOfFun, this query works for all SymID-s that represent functions,
	 * be it user-defined, extern, built-in, or generated.
	 */
	DECLARE_QUERY(QueryDeclOfFun, SymID, CRef<HOUTFunctionDeclaration>);

	/**
	 * @brief Query code of a function.
	 * @note Works only for SymID-s that actually represent PST-function (i.e. PST symbol).
	 */
	DECLARE_QUERY(QueryCodeOfFun, SymID, HOUTFunction);

	/**
	 * @brief Query all function dependencies of a function (e.g. for a given function SymID, return
	 * all SymID-s of this function.
	 * @note Works only for SymID-s that actually represent PST-function (i.e. PST symbol).
	 */
	DECLARE_QUERY(QueryDirectFunctionCalls, SymID, std::vector<SymID>);

	/**
	 * @brief Query all function dependencies of a function (e.g. SymID-s of all functions called by
	 * this function or all functions called by the called functions).
	 * @note This query is used to determine all other functions that have to be compiled when
	 * compile time evaluating a function.
	 * @note Works only for SymID-s that actually represent PST-function (i.e. PST symbol).
	 */
	DECLARE_QUERY(QueryTransitiveFunctionCalls, SymID, std::vector<SymID>);
}
