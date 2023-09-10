#include "symbol.hpp"
#include "../analysis_state.hpp"

namespace symtable {
	
	Symbol::Symbol(ScopeRef scope, base::StrId name, bool anonymous,
	               bool is_static, SymbolKind kind):
		scope(scope),
		name(name),
		anonymous(anonymous),
		is_static(is_static),
		relative_position(-1),
		kind(kind)
		{}

	ts::TypeDesc<> Symbol::getType() {
		if (!type.has_value()) {
			calculateType();
		}
		return type.value();
	}

	exec::CTV Symbol::getValue() {
		RIFT_PANIC("getValue called on Symbol not implemeting it");
	}

	ScopeRef Symbol::getLinkedLookupScope(hir::AnalysisState& state) {
		if (!linked_lookup_scope.has_value()) {
			calculateLinkedLookup(state);
		}
		return linked_lookup_scope.value();
	}

	void Symbol::calculateLinkedLookup(hir::AnalysisState& state) {
		linked_lookup_scope = state.newScope(scope);
	}

	// @TODO: errors
	LookupResult Symbol::lookupIn(hir::AnalysisState& state, base::StrId name)  {
		scope = getLinkedLookupScope(state);
		return scope->lookup(state, name);
	}

	SymbolChain Symbol::getUniqueDeAlias(hir::AnalysisState&) {
		if (is_alias) {
			RIFT_PANIC("de alias called on wildcard symbol not implementing deAlias");
		}
		return { SymbolRef(this) };
	}
	
	ChainLookupResult Symbol::getDeAlias(hir::AnalysisState&) {
		if (is_alias) {
			RIFT_PANIC("de alias called on alias symbol not implementing deAlias");
		}
		return {{}, {{SymbolRef(this)}, {}}};
	}
}
