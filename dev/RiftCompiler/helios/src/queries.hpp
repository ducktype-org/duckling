#pragma once

#include <query_framework/query_int.hpp>

// @TODO: relax this dependency
#include <frontend/module_tree/queries.hpp>

#include <vector>
#include "scope_symbol_id.hpp"
#include "hout/hout.hpp"

namespace compiler::helios {
	// @FUTURE: paraph we will need to add more granularity to HOUT generation for efficient incremental compilation

	/**
	 * @brief Query FULL HOUTModule of single module
	 */
	DECLARE_QUERY(QueryModuleHOUT, frontend::ModuleId, HOUTModule)
	
	/**
	 * @brief Query FULL HOUTModule of module and all its submodules recursively
	 */
	DECLARE_QUERY(QueryModuleHOUTRecursively, frontend::ModuleId, std::vector<HOUTModule>)

	/**
	 * @brief Query SymbolID of given module
	 */
	DECLARE_QUERY(QueryModuleSymbol, frontend::ModuleId, SymID)


}

