#pragma once

#include <query_framework/query_int.hpp>
#include <frontend/module_tree/queries.hpp>
#include <vector>
#include "hout/hout.hpp"

namespace compiler::helios {
	// what query we want:
	// - ModuleID -> HOUT (entry point, only thing that is technically needed)
	//   - it has to steal the "HOUT" -- it will be builded as global state that is then moved? 
	//   - ModuleID -> SymID


	// internally:
	// Symbols:
	//  - Compile SymID (what does it returns...? nothing? side-effect? its HOUT?)
	//  - type of SymID
	//  - value of SymID
	//  - lookup in SymID (+ lookup int type)
	//  - dealias SymID -> [SymID]

	// Scopes:
	// - [SymID] -> ScopeID
	// - lookup in ScopeID (Str, ScopeID -> LookupResult)

	// inside will be:
	// - symbols stable container
	// - scopes stable container
	// will operate on ScopeRef SymbolRef (stuff hidden from end user, looks like ID)

	// additional structures:
	// - HOUT
	// - Lookup result

	// for now: copy stuff around and leave @OPT for -- make stable cache here and return reference

	// we migth want to refactor lookup result..


	/**
	 * @brief Generates HOUTModule of single module
	 */
	DECLARE_QUERY(QueryModuleHOUT, frontend::ModuleId, HOUTModule)
	
	/**
	 * @brief Generates HOUTModule of module and all its submodules recursively
	 */
	DECLARE_QUERY(QueryModuleHOUTRecursively, frontend::ModuleId, std::vector<HOUTModule>)
}

