#pragma once

#include <vector>
#include "scope_symbol_id.hpp"
#include "symbol_ref.hpp"
#include "lookup_result.hpp"
#include <base/string_id.hpp>
#include <set>

namespace symtable {
	enum class ConnectionType {
		ImportPublic,
		ImportPrivate,
		UsingPublic,
		UsingPrivate
	};

	class Scope {
		// Update constructors when adding fields here:
		
		// @TODO: option
		ScopeRef parent;
		// ScopeId id;

		// This name is for debug only:
		base::StrId name;

		std::vector<SymbolRef> symbols;

		// bool lookup_engaged = false;
		std::set<base::StrId> engaged_names;

		friend class SymbolData;

		Scope() = default;
		Scope(ScopeRef parent, base::StrId name): parent(parent), name(name) {}

	public:
		// This delete is important, to prevent any copy of scope data:
		Scope(const Scope&) = delete;
		Scope & operator=(const Scope&) = delete;

		const std::vector<SymbolRef>& getSymbols() { return symbols; };
		ScopeRef getParent() { return parent; }
		
		Scope(Scope&&) = default;
		Scope& operator=(Scope&&) = default;

		LookupResult lookup(base::StrId name);
		LookupResult lookupMeAndParents(base::StrId name);

		base::StrId getName() const { return name; }
	};

}