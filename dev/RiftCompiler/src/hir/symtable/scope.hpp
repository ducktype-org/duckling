#pragma once

#include "lookup_result.hpp"
#include "scope_symbol_id.hpp"
#include "symbol_ref.hpp"

#include <base/string_id.hpp>
#include <vector>

namespace hir {
	class AnalysisState;
}

namespace symtable {
	enum class ConnectionType { ImportPublic, ImportPrivate, UsingPublic, UsingPrivate };

	class Scope {
		// Update constructors when adding fields here:

		// @TODO: option
		ScopeRef parent;
		// ScopeId id;

		std::vector<SymbolRef> symbols;

		bool lookup_engaged = false;

		friend class SymbolData;

		Scope() = default;

		Scope(ScopeRef parent): parent(parent) {}

	public:
		// This delete is important, to prevent any copy of scope data:
		Scope(const Scope&)            = delete;
		Scope& operator=(const Scope&) = delete;

		const std::vector<SymbolRef>& getSymbols() { return symbols; }

		ScopeRef getParent() { return parent; }

		Scope(Scope&&)            = default;
		Scope& operator=(Scope&&) = default;

		LookupResult lookup(hir::AnalysisState&, base::StrId name);
		LookupResult lookupMeAndParents(hir::AnalysisState&, base::StrId name);
	};

}
