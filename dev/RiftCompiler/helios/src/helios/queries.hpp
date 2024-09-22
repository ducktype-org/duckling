/**
 * @file queries.hpp
 * @brief This file contains top-level queries for interacting with HELIOS.
 * @note Other queries related to Symbols and Scopes can albo be called from outside HELIOS.
 */
#pragma once

#include <query_framework/query_int.hpp>

// @TODO: relax this dependency
#include <frontend/module_tree/queries.hpp>

#include <vector>

#include "scope_symbol_id.hpp"
#include "hout/hout.hpp"

namespace compiler::helios {
	// @FUTURE: paraph we will need to add more granularity to HOUT generation for efficient
	// incremental compilation

	/**
	 * @brief Debug function to print scope and its parents IDs.
	 * @note: not used right now
	 * @param scope
	 */
	void printScopeAndParents(ScopeID scope);

	/**
	 * @brief Query FULL HOUTUnit of single module
	 */
	DECLARE_QUERY(QueryModuleHOUT, frontend::ModuleID, const HOUTUnit&)

	/**
	 * @brief Query HOUTUnit of module and all its submodules recursively
	 */
	DECLARE_QUERY(QueryModuleHOUTRecursively, frontend::ModuleID, std::vector<HOUTUnit>)

	/**
	 * @brief Debug/testing query for extracting top-level functions and constants from module
	 */
	DECLARE_QUERY(QueryTopLevelEntities, frontend::ModuleID, HOUTUnit)

	/**
	 * @brief Query code of a function.
	 * @note Works only for SymID-s that actually represent a function
	 */
	DECLARE_QUERY(QueryCodeOFFun, SymID, HOUTFunction);
}
