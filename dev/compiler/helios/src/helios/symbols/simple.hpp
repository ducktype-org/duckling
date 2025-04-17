/**
 * File for various simple, non-query operations on SymID.
 * Implements it inside symbols.cpp in src_private.
 */

#pragma once

#include "symbol_kind.hpp"

#include <helios/scope_symbol_id.hpp>
#include <pst_parser/access.hpp>
#include <pst_parser/elements/elements.hpp>  // @TODO #404 relax it somehow

#include <base/string_id.hpp>

namespace compiler::helios {
	// Following functions are left as functions (instead of beeing a query):
	// in the future once Query System implementation will mature
	// they will probably be have to be converted into queries
	// from DefID/PstID to appropriate data:

	/**
	 * @return is given symbol a wildcard symbol (e.g. using a.*)
	 */
	bool isWildcard(SymID);

	/**
	 * @return name of the symbol
	 */
	base::StrID name(SymID);

	/**
	 * @return kind of the symbol
	 */
	SymbolKind kind(SymID);

	/**
	 * @return scope that given symbol was defined within
	 */
	ScopeID scope(SymID);

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
	pst::AccessLocked<pst::LangElement> symbolPst(SymID);

}
