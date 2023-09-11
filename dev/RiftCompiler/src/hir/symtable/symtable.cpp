#include "symtable.hpp"
#include <base/exceptions.hpp>
#include <base/defer.hpp>
#include <iostream>

namespace symtable {

	SymbolData::SymbolData() {
		root_scope = newScope(nullptr, base::StrId("ROOT_SCOPE"));
		next_relative_position = 0;
	}

	ScopeRef SymbolData::getRootScope() {
		return root_scope;
	}

	ScopeRef SymbolData::newScope(ScopeRef parent, base::StrId name) {
		auto id = scopes.pushBack(std::move(Scope(parent, name)));
		// scopes[id].id = id;
		return scopes.last();
	}

	ScopeRef SymbolData::newSubRootScope() {
		return newScope(getRootScope(), base::StrId("SUB_ROOT_SCOPE"));
	}

	SymbolRef SymbolData::newSymbol(base::unique_ptr<Symbol> symbol) {
		auto scope = symbol->getScope();	
		
		symbol->relative_position = next_relative_position++;
		
		symbols.push_back(std::move(symbol));
		scope->symbols.push_back(symbols.back().borrow_mut());

		return symbols.back().borrow_mut();
	}


	usize SymbolData::symbolCount() const {
		return symbols.size();
	}

	const decltype(SymbolData::symbols)& SymbolData::getSymbols() const {
		return symbols;
	}

	// @TODO: errors
	ChainLookupResult SymbolData::lookupDottedNameInScopeAndParents(
		hir::AnalysisState& state, ScopeRef initial,
		std::span<base::StrId> names) {
		
		RIFT_ASSERT(names.size() > 0, "lookupDotted received zero names");

		// initial symbol:
		auto append_res_first = initial->lookupMeAndParents(state, names[0]);

		if (names.size() == 1) {
			return {{}, append_res_first};
		}

		if (!append_res_first.isSingle()) {
			// @TODO: error in state
			// return some „ErrorSymbol”
			RIFT_PANIC("ambiguity in lookup");
		}

		// ChainLookupResult result;
		SymbolChain prefix = append_res_first.getAsSingle();

		for (usize i = 1; i < names.size() - 1; i++) {
			auto append_res = prefix.back()->lookupIn(state, names[i]);
			if (!append_res.isSingle()) {
				// @TODO: error in state
				// return some „ErrorSymbol”
				RIFT_PANIC("ambiguity in lookup");
			}

			auto single_append_res = append_res.getAsSingle();

			prefix.insert(prefix.end(), single_append_res.begin(), single_append_res.end());
		}

		// last symbol:
		auto last_res = prefix.back()->lookupIn(state, names.back());

		return {prefix, last_res};
	}

}
