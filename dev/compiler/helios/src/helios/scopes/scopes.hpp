/**
 * @file scopes.hpp
 * @brief This file defines Queries responsible for creation of Scopes and operations on them.
 *
 * What are created for given PST element (list only for elements that have their own scope):
 * * TopLevel: Scope containing all top-level statements in the file.
 * * CodeBlockOrStmt: Scope containing all statements in the block.
 * * CodeBlock: Scope containing all statements in the block.
 *   Unless it is contained in CodeBlockOrStmt, then it does not have a scope.
 * * ClassBlock: Scope containing all statements in the class.
 * * If, While, For: Scope for symbols defined in the condition/iteration declaration.
 * * Fun, ClassMethod: Scope for function parameters.
 *
 * * ExprStmt: Scope for expresion lifetime
 *   (this will change in the future, we will just have scope for top-exprs).
 *
 * @note all symbols need a scope. If we don't have one, we should add it.
 */
#pragma once

#include <base/string_id.hpp>
#include <query_framework/query_int.hpp>
#include <base/perfect_hash.hpp>
#include <pst_parser/elements/elements.hpp>
#include <pst_parser/lang_parser_state.hpp>
#include <pst_parser/generic_query_key.hpp>

#include "../scope_symbol_id.hpp"
#include "../lookup_result.hpp"

// @TODO: relax this dependency (we only need ModuleID in hpp) (#404)
#include <frontend/module_tree/queries.hpp>

namespace compiler::helios {
	/**
	 * @brief Return parent scope or none for root-scopes.
	 */
	base::Optional<ScopeID> parent(ScopeID);

	/**
	 * @brief Return module the scope was defined in
	 */
	frontend::ModuleID module(ScopeID id);

	/**
	 * @brief Return depth of the scope in the scope tree.
	 */
	u64 scopeDepth(ScopeID);

	/**
	 * @brief Return all scopes currently stored by HELIOS.
	 * @note: This should be used for tests and debug only.
	 * @return std::vector<ScopeID>
	 */
	std::vector<ScopeID> getAllHeliosScopes();

	/**
	 * @brief Query root scope for given module.
	 * @todo: Right now RootScopes are empty, and in order to access proper module
	 * symbols, one need to get scope of root element of the main module file.
	 * This should be somehow refactored when multi-file modules will be introduced.
	 * @todo: Currently root scopes are somewhat problematic.
	 * See description of "root_element_file_back_map" for details.
	 */
	DECLARE_QUERY(QueryRootScopeOf, frontend::ModuleID, ScopeID);

	/**
	 * @brief Generate HELIOS-scope associated with given PST element.
	 * Also: dictates what PST elements have their own scope.
	 *
	 * For some elements (e.g: code-block) this will be a scope of the element itself.
	 * For some it will be a scope this element is contained in (e.g. inner expression elements).
	 * For some (e.g: function) this might be slightly different.
	 * For some elements for which scope does not make sense, it can panic.
	 *
	 * @note Scope strucure are linked directly to PST structure.
	 * The reason for this is that handling scope structure without direct link to PST was highly
	 * bug prone and led to potential errors or lack of consistency between different fragments of
	 * code.
	 */
	DECLARE_QUERY(QueryPrimaryCodeScopeFor, pst::GenericPSTQueryKey<>, ScopeID);

	/**
	 * @brief A helper function, to make scope API consistent.
	 * Generate scope of the body for given PST stmt element.
	 * Intuitively this is a scope, that you associate with given element,
	 * when looking at the code (think of namespaces for example).
	 */
	ScopeID queryBodyCodeScopeFor(query::Context&, pst::AccessLocked<pst::Stmt> stmt);

	struct KeyOf_LookupInScope final {
		ScopeID     scope;
		base::StrID name;
		bool        with_wildcards;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
		bool        operator==(const KeyOf_LookupInScope&) const = default;
	};

	/**
	 * @brief Performs lookup of single name inside given scope.
	 */
	DECLARE_QUERY(QueryLookupInScope, KeyOf_LookupInScope, CRef<LookupResult>);

	/**
	 * @brief Performs lookup of single name inside given scope and its parents.
	 */
	DECLARE_QUERY(QueryLookupInScopeAndParents, KeyOf_LookupInScope, CRef<LookupResult>);

	/**
	 * @brief Query all symbols that are directly inside given scope.
	 * Also: dictates what symbols are contained in what scopes.
	 */
	DECLARE_QUERY(QuerySymbolsInScope, ScopeID, CRef<std::vector<SymID>>);

	/**
	 * @brief Root scope of main module file.
	 * It is currently the "effective" root scope of a module.
	 * See: QueryRootScope for details
	 * @todo: this has to change in the future
	 *
	 * @param module
	 * @return ScopeID
	 */
	ScopeID queryRootScopeOfMainModuleFile(query::Context&, frontend::ModuleID module);
}
