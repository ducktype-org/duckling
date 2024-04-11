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
	};
	struct ScopeID {
	private:
		base::borrow_ptr<ScopeData> ref;
	};

	
}
