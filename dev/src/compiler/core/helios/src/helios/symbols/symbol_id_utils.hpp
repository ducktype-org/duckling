/**
 * @file symbol_id_utils.hpp
 * File for various simple, non-query operations on SymID.
 * Implements it inside symbols.cpp in src_private.
 */

#pragma once

#include "symbol_id.hpp"
#include "symbol_kind.hpp"

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/elements_list.hpp>
#include <helios/scope_id.hpp>

#include <string_id/string_id.hpp>

namespace compiler::helios {
	// Following functions are left as functions (instead of beeing a query):
	// in the future once Query System implementation will mature
	// they will probably be have to be converted into queries
	// from DefID/PstID to appropriate data:

	/**
	 * @return is given symbol a wildcard symbol (e.g. using a.*)
	 */
	bool isWildcard(SymID);

	bool isAlias(SymID);

	/**
	 * @return name of the symbol
	 */
	base::StrID name(SymID);

	/**
	 * @return whether SymID is a global function.
	 * @note This function iterates through parents of the PST elements of the symbol.
	 */
	bool isGlobalFun(query::Context&, SymID);

	/**
	 * @return whether SymID is a global variable.
	 * @note This function iterates through parents of the PST elements of the symbol to obtain this
	 * information. It might be changed in the future, especially when more kinds of global
	 * variables will appear (for example analog to C++ static variables).
	 */
	bool isGlobalVar(query::Context&, SymID);

	/**
	 * @return kind of the symbol
	 */
	SymbolKind kind(SymID);

	/**
	 * @return scope that given symbol was defined within.
	 * Throws in symbol doesn't have a scope.
	 */
	ScopeID scope(query::Context&, SymID);

	/**
	 * Gets scope that given symbol was defined within.
	 * Returns empty optional if the symbol doesn't have a scope.
	 * E.g. builtin functions don't have a scope.
	 */
	base::Optional<ScopeID> maybeScope(query::Context& ctx, SymID);

	/**
	 * @return PST Stmt element symbol was created from.
	 * Panics if the element was not a statement.
	 * @todo should this be an external API? It might depend on incremental compilation
	 * implementation
	 */
	base::Optional<pst::Access<pst::Stmt>> stmt(query::Context&, SymID);

	/**
	 * @return PST element symbol was created from.
	 */
	base::Optional<pst::AccessLocked<pst::LangElement>> symbolPst(SymID);

	/**
	 * @return PST element symbol was created from,
	 * or empty optional if the symbol was not created from a PST element.
	 */
	base::Optional<pst::AccessLocked<pst::LangElement>> maybeSymbolPst(SymID id);

	/**
	 * @brief Pretty prints the symbol.
	 */
	std::string prettyDebugPrint(SymID, query::Context&);
}
