#pragma once

#include <base/string_id.hpp>
#include <base/named_id.hpp>
#include <base/maps.hpp>
#include <base/smart_pointers.hpp>
#include "scope_symbol_id.hpp"
#include "symbol.hpp"
#include "scope.hpp"
#include "symbol_ref.hpp"
#include "lookup_result.hpp"

#include <base/ints.hpp>
#include <span>
#include <ostream>
#include <base/stable_container.hpp>

namespace symtable {


	using detail::ScopesList;

	// using detail::SymbolList;

	/**
	 * @brief SymbolData stores all symbols and the scope tree.
	 * Is also provides an interface for the creation of new scopes.
	 */
	class SymbolData {
		ScopeRef root_scope;

		ScopesList                            scopes;
		std::vector<base::unique_ptr<Symbol>> symbols;

		usize next_relative_position;

	public:
		SymbolData();
		ScopeRef getRootScope();
		ScopeRef newScope(ScopeRef parent, base::StrId name);
		ScopeRef newSubRootScope();

		// @TODO: This mechanism is a little bit weird:
		SymbolRef newSymbol(base::unique_ptr<Symbol> symbol);

		const decltype(symbols)& getSymbols() const;

		usize symbolCount() const;


		/**
		 * @note Naive implementations for now.
		 * @TODO: add consts
		 * @deprecated this should not be here. It should be one of: a function, member of Scope
		 * class, member of some agent responsible for "lookup".
		 */
		ChainLookupResult
			lookupDottedNameInScopeAndParents(ScopeRef initial, std::span<base::StrId> names);
	};
}
