#include "symbol.hpp"
#include "../analysis_state.hpp"

namespace symtable {

	Symbol::Symbol(
		hir::AnalysisState& state,
		ScopeRef            scope,
		base::StrId         name,
		bool                anonymous,
		bool                is_static,
		SymbolKind          kind
	):
		  analysis_state(state),
		  scope(scope),
		  name(name),
		  anonymous(anonymous),
		  is_static(is_static),
		  relative_position(-1),
		  kind(kind) {}

	ts::TypeDesc<> Symbol::requestType() {
		if (!type.has_value()) calculateType();
		return type.value();
	}

	exec::CTV Symbol::requestValue() {
		RIFT_PANIC("getValue called on Symbol not implementing it");
	}

	ScopeRef Symbol::requestLinkedLookupScope() {
		if (!linked_lookup_scope.has_value()) {
			RIFT_ASSERT(unlockedLookup(), "Trying to requestLinkedLookup while in lookup lock");
			calculateLinkedLookup();
		}
		return linked_lookup_scope.value();
	}

	void Symbol::getSymbolsIn() { symbol_in_done = true; }

	void Symbol::calculateLinkedLookup() {
		linked_lookup_scope = analysis_state.newScope(scope, name);

		// @TODO: this here is not perfect, but it guarantees,
		// that Scope always has symbols
		// In the future there should be some link from Scope to symbol
		// and going over symbols will be done only when necessary
		getSymbolsIn();
		linked_lookup_scope.value()->close();
	}

	// @TODO: errors
	LookupResult Symbol::requestLookupIn(base::StrId name) {
		RIFT_ASSERT(unlockedLookup(), "Trying to lookupIn while in locked lookup state");

		scope = requestLinkedLookupScope();
		getSymbolsIn();
		return scope->lookup(name);
	}

	SymbolChain Symbol::requestUniqueDeAlias() {
		if (is_alias) RIFT_PANIC("de alias called on wildcard symbol not implementing deAlias");
		return { SymbolRef(this) };
	}

	ChainLookupResult Symbol::requestDeAlias() {
		if (is_alias) RIFT_PANIC("de alias called on alias symbol not implementing deAlias");
		return { {}, { { SymbolRef(this) }, {} } };
	}

	bool Symbol::unlockedLookup() const { return not lock_lookup; }
}
