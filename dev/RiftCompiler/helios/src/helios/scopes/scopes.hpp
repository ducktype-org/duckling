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
	using StmtList = std::vector<PstRef<pst::Stmt>>;

	// this is just a mock:
	std::string dprint(ScopeID);

	DECLARE_QUERY(QueryRootScopeOf, frontend::ModuleId, ScopeID);

	struct KeyOf_QueryPrimaryCodeScopeFor {
		// @TODO is this needed?
		ScopeID parent;

		// Stmt here makes no sense with getChildStmtsOf
		// The overall idea is fine, but needs some polishing 
		PstRef<pst::Stmt> base_element;
		// StmtList stmts;

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

	struct KeyOf_LookupInScope {
		ScopeID scope;
		base::StrId name;

		base::HashT customPerfectHash() const;
		bool operator==(const KeyOf_LookupInScope&) const = default;
	};
	DECLARE_QUERY(QueryLookupInScope, KeyOf_LookupInScope, const LookupResult&);


	DECLARE_QUERY(QuerySymbolsInScope, ScopeID, const std::vector<SymID>&);

	// DECLARE_QUERY(QueryLookupInScopeAndParents, KeyOf_Lookup, LookupResult);

}
