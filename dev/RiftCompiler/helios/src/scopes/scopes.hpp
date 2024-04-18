#pragma once

#include "../scope_symbol_id.hpp"
#include "../lookup_result.hpp"
#include <base/string_id.hpp>
#include <query_framework/query_int.hpp>
#include "../pst_ref.hpp"
#include "base/perfect_hash.hpp"
#include "pst_parser/elements/elements.hpp"
#include "pst_parser/rift_parser_base.hpp"

// @TODO: relax this dependency
#include <frontend/module_tree/queries.hpp>

namespace compiler::helios {
	// queries:
	// 
	// unlike in old-hir, here queries will have to have some kind of link to Pst to it can calculate its symbols

	// @TODO: hmm, this is kind of problematic, because SuperRootScope has to contain all module-scopes or something like that..
	// So: first create root module, then create super-root-scope with that symbol
	// but then... what scope does root module is in?

	using StmtList = std::vector<PstRef<pst::Stmt>>;

	DECLARE_QUERY(QuerySuperRootScope, std::vector<frontend::ModuleId>, ScopeID);
	
	DECLARE_QUERY(QueryRootScopeOf, frontend::ModuleId, ScopeID);

	// @TODO: how to fill scopes? -- two options:
	// 1. Do it on creation
	// 2. do it elsewhere -- how? 
	

	struct KeyOf_QueryPrimaryCodeScopeFor {
		PstRef<pst::RiftElement> base_element;
		StmtList stmts;

		// @TODO:
		base::HashT customPerfectHash() const;
	};
	/**
	 * @brief Construct a Scope that has following properties:
	 *   * parent of the scope is the scope in which the RiftElement is
	 *   * is unique for that symbol
	 */
	DECLARE_QUERY(QueryPrimaryCodeScopeFor, KeyOf_QueryPrimaryCodeScopeFor, ScopeID);

	// struct KeyOf_QueryNthScopeIn {
	// 	ScopeID parent;
	// 	u64 number;
	// };
	// /**
	//  * @brief Construct a Scope that is n-th generic child of parent.
	//  * It might be necessary for construction of auxilary scopes
	//  */
	// DECLARE_QUERY(QueryNthScopeIn, KeyOf_QueryNthScopeIn, ScopeID);

	struct KeyOf_Lookup {
		ScopeID scope;
		base::StrId name;
	};
	DECLARE_QUERY(QueryLookupInScope, KeyOf_Lookup, LookupResult);

	DECLARE_QUERY(QueryLookupInScopeAndParents, KeyOf_Lookup, LookupResult);

}
