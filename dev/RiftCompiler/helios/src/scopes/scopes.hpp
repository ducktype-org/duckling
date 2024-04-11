#pragma once

#include "../scope_symbol_id.hpp"
#include <query_framework/query_int.hpp>

// @TODO: relax this dependency
#include <frontend/module_tree/queries.hpp>

namespace compiler::helios {
	// queries:
	// 
	// unlike in old-hir, here queries will have to have some kind of link to Pst to it can calculate its symbols

	DECLARE_QUERY(QuerySuperRootScope, query::EmptyKey, ScopeID);
	DECLARE_QUERY(QueryRootScopeOf, frontend::ModuleId, ScopeID);
	
	
	struct KeyOf_QueryCodeScope {
		// @TODO: hmm
		// for sure: parent, some (potentially indirect) link to Pst so all symbols can be grabbed
	};
	DECLARE_QUERY(QueryCodeScope, KeyOf_QueryCodeScope, ScopeID);

}
