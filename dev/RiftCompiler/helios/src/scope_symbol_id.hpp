#pragma once

#include <base/stable_container.hpp>

namespace compiler::helios {
	// Forward:
	struct SymbolData;
	struct ScopeData;

	/**
	 * @brief Reference to HELIOS-symbol
	 */
	using SymbolRef = base::borrow_ptr<SymbolData>;
	using ScopeRef = base::borrow_ptr<ScopeData>;

	struct SymID {
	private:
		SymbolRef ref;
	};
	struct ScopeID {
	private:
		ScopeRef ref;
	};

	
}
