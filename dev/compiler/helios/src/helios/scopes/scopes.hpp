/** @file scopes.hpp
 *  @brief This file defines Queries responsible for creation of Scopes and operations on them.
 */
#pragma once

#include <base/string_id.hpp>
#include <query_framework/query_int.hpp>
#include <base/perfect_hash.hpp>
#include <pst_parser/elements/elements.hpp>
#include <pst_parser/lang_parser_state.hpp>

#include "../scope_symbol_id.hpp"
#include "../lookup_result.hpp"

// @TODO: relax this dependency
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

	struct KeyOf_QueryCodeScopeFor final {
		/**
		 * @brief Element for which the scope is created.
		 * @note: scopes of various elements behave differently
		 * Scope behaviour for:
		 * * StatementAggregates -- a scope of aggregated statements
		 * * Function -- a scope of function arguments (@todo: function scopes are currently empty)
		 * * Namespaces -- empty Scope
		 * * Classes -- scope containing class fields
		 * * Expr -- empty Scope
		 * * Return -- empty Scope
		 * * Variables -- empty Scope
		 *
		 * @todo: once scope refactor will be introduced, most "empty scope"
		 * stuff will be no longer needed.
		 */
		MCRef<pst::LangElement> base_element;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
		bool        operator==(const KeyOf_QueryCodeScopeFor&) const = default;
	};

	/**
	 * @brief Query Primary Scope for given PST element.
	 * For some elements (e.g: code-block) this will be a scope of the element itself.
	 * For some it will be a scope this element is contained in (e.g. inner expression elements).
	 * For some (e.g: function) this might be slightly different.
	 *
	 * For some elements for which scope does not make sense, it can panic.
	 * 
	 * @note: Scope strucure are linked directly to PST structure.
	 * The reason for this is that handling scope structure without direct link to PST was highly
	 * bug prone and led to potential errors or lack of consistency between different fragments of
	 * code.
	 */
	DECLARE_QUERY(QueryPrimaryCodeScopeFor, KeyOf_QueryCodeScopeFor, ScopeID);

	struct KeyOf_QueryCodeScopeForStmt final {
		MCRef<pst::Stmt> base_element;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
		bool        operator==(const KeyOf_QueryCodeScopeForStmt&) const = default;
	};

	/**
	 * @brief Query Primary so called Intuitive for given PST stmt element.
	 * Intuitively this is a scope, that you associate with given element,
	 * when looking at the code (think of namespaces for example).
	 */
	DECLARE_QUERY(QueryIntuitiveCodeScopeFor, KeyOf_QueryCodeScopeForStmt, ScopeID);

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
	 *
	 * @NOTE: For structs, it returns what's inside struct's body.
	 */
	DECLARE_QUERY(QuerySymbolsInScope, ScopeID, CRef<std::vector<SymID>>);

	/**
	 * @brief Return the scope, that symbol created from given PST element
	 * Should be in.
	 * @note This has to be consistant with QuerySymbolsInScope
	 */
	ScopeID getPSTElementParentScope(query::Context&, MCRef<pst::LangElement> element);

	/**
	 * @brief Root scope of main module file is currently the "effective" root scope.
	 * See: QueryRootScope for details
	 * @todo: this has to change in the future
	 *
	 * @param module
	 * @return ScopeID
	 */
	ScopeID queryRootScopeOfMainModuleFile(query::Context&, frontend::ModuleID module);
}
