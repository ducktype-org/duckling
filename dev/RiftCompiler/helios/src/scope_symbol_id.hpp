#pragma once

#include <base/stable_container.hpp>

namespace compiler::helios {
	// Forward:
	// @TODO: put in detail?
	struct SymbolData;
	struct ScopeData;

	struct SymID {
	private:
		base::borrow_ptr<SymbolData> ref;
		SymID(base::borrow_ptr<SymbolData> ref): ref(ref) {}
		friend struct ImplementationOf_QuerySymbolOfSTMT;
	};
	struct ScopeID {
	private:
		base::borrow_ptr<ScopeData> ref;
		ScopeID(base::borrow_ptr<ScopeData> ref): ref(ref) {}
		friend struct ImplementationOf_QuerySuperRootScope;
	};

	
}
