#pragma once

#include <vector>
#include "scope_symbol_id.hpp"
#include "symbol_ref.hpp"
#include "lookup_result.hpp"
#include <base/string_id.hpp>
#include <set>

namespace symtable {
	// @deprecated
	// enum class ConnectionType { ImportPublic, ImportPrivate, UsingPublic, UsingPrivate };

	/**
	 * @brief Stores current state of a scope
	 */
	enum class ScopeState {
		Open, ///< Open state means that symbols can be added to the scope.
		Closed, ///< Close state means that lookup can be performed inside the scope.
	};

	/**
	 * @brief Scope represents a single source-code scope
	 * with a list of symbols in it.
	 */
	class Scope {
		// Update constructors when adding fields here:

		// @TODO: option
		ScopeRef parent;
		// ScopeId id;

		// This name is for debug only:
		base::StrId name;

		// List of symbols inside the scope.
		std::vector<SymbolRef> symbols;

		// This is somewhat buggy way of preventing lookup cycles
		std::set<base::StrId> engaged_names;

		ScopeState state = ScopeState::Open;

		friend class SymbolData;
		void addSymbol(SymbolRef symbol);

		Scope() = default;

		Scope(ScopeRef parent, base::StrId name): parent(parent), name(name) {}

	public:
		// This delete is important, to prevent any copy of scope data:
		Scope(const Scope&)            = delete;
		Scope& operator=(const Scope&) = delete;

		const std::vector<SymbolRef>& getSymbols() { return symbols; }

		ScopeRef getParent() { return parent; }

		Scope(Scope&&)            = default;
		Scope& operator=(Scope&&) = default;

		LookupResult lookup(base::StrId name);
		LookupResult lookupMeAndParents(base::StrId name);

		base::StrId getName() const { return name; }

		/**
		 * @brief In order to perform lookup one must close the scope.
		 * After the scope is closed no more symbols can be added to the scope.
		 */
		void close();
	};

}
