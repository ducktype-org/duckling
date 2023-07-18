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

#include <cstdint>
#include <span>
#include <ostream>
#include <base/stable_container.hpp>

namespace symtable {


	using detail::ScopesList;
	// using detail::SymbolList;

	class SymbolData {
		ScopeRef root_scope;

		ScopesList scopes;
		std::vector<base::unique_ptr<Symbol>> symbols;

		size_t next_relative_position;

	public:
		SymbolData();
		ScopeRef getRootScope();
		ScopeRef newScope(ScopeRef parent);
		ScopeRef newSubRootScope();
		SymbolRef newSymbol(base::unique_ptr<Symbol> symbol);

		const decltype(symbols)& getSymbols() const;

		size_t symbolCount() const;


		// @TODO: add consts
		// naive implementations for now:
		ChainLookupResult lookupDottedNameInScopeAndParents(hir::AnalysisState&, ScopeRef initial, std::span<base::StrId> names);
	};
}
