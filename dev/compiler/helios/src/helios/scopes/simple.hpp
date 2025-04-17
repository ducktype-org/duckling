// @TODO: this can be moved back to src_private once #637 is merged
// .cpp part is intentionally left in src_private in scopes.cpp

#pragma once

#include <helios/scope_symbol_id.hpp>
#include <frontend/module_tree/module_id.hpp>

namespace compiler::helios {
    /**
	 * @brief Return parent scope or none for root-scopes.
	 */
	base::Optional<ScopeID> parent(ScopeID);

	/**
	 * @brief Return module the scope was defined in
	 */
	frontend::ModuleID module(ScopeID id);

	/**
	 * @brief Return depth of the scope in the scope tree.
	 */
	u64 scopeDepth(ScopeID);
}

