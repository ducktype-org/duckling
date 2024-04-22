#pragma once

#include "base/perfect_hash.hpp"
#include <base/stable_container.hpp>

namespace compiler::helios {
	// Forward:
	// @TODO: put in detail?
	struct SymbolData;
	struct ScopeData;

	struct SymID {
		// @FUTURE: add some mangling, so valgrind will not get confused
		base::HashT customPerfectHash() const { return reinterpret_cast<u64>(ref.get()); };
		bool operator==(const SymID&) const = default;
	private:
		base::borrow_ptr<SymbolData> ref;
		SymID(base::borrow_ptr<SymbolData> ref): ref(ref) {}
		friend struct ImplementationOf_QuerySymbolOfSTMT;
		friend struct GetSymRef_Functor;
	};

	struct ScopeID {
		// @FUTURE: add some mangling, so valgrind will not get confused
		base::HashT customPerfectHash() const { return reinterpret_cast<u64>(ref.get()); };
		bool operator==(const ScopeID&) const = default;
	private:
		base::borrow_ptr<ScopeData> ref;
		ScopeID(base::borrow_ptr<ScopeData> ref): ref(ref) {}
		friend struct ImplementationOf_QuerySuperRootScope;
		friend struct ImplementationOf_QueryPrimaryCodeScopeFor;
		friend struct ImplementationOf_QuerySymbolsInScope;
		friend struct GetScopeRef_Functor;
	};


}
