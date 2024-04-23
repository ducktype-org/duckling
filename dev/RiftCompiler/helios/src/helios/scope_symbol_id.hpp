#pragma once

#include "base/perfect_hash.hpp"
#include <base/stable_container.hpp>
#include <utility>

namespace compiler::helios {
	// Forward:
	// @TODO: put in detail?
	struct SymbolData;
	struct ScopeData;

	struct SymID {
		// @FUTURE: add some mangling, so valgrind will not get confused
		base::HashT customPerfectHash() const { return reinterpret_cast<u64>(ref.get()); }

		bool operator==(const SymID&) const = default;

	private:
		base::borrow_ptr<SymbolData> ref;

		SymID(base::borrow_ptr<SymbolData> ref): ref(ref) {}
		friend struct ImplementationOf_QuerySymbolOfSTMT;
		friend struct ImplementationOf_QueryLookupInSymbol;
		friend struct GetSymRef_Functor;
		friend struct ImplementationOf_QueryLinkedScope;
	};

	struct ScopeID {
		// @FUTURE: add some mangling, so valgrind will not get confused
		[[nodiscard]] base::HashT customPerfectHash() const { return reinterpret_cast<u64>(ref.get()); }

		bool operator==(const ScopeID&) const = default;

	private:
		base::borrow_ptr<ScopeData> ref;

		ScopeID(base::borrow_ptr<ScopeData> ref): ref(std::move(ref)) {}
		friend struct ImplementationOf_QueryRootScopeOf;
		friend struct ImplementationOf_QueryPrimaryCodeScopeFor;
		friend struct ImplementationOf_QuerySymbolsInScope;
		friend struct ImplementationOf_QueryLookupInScopeAndParents;
		friend struct GetScopeRef_Functor;
	};


}
