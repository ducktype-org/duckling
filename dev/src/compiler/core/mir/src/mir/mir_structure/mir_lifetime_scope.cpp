#include "mir_lifetime_scope.hpp"

namespace compiler::mir {

	namespace {
		/**
		 * Simple counter for generating unique scope IDs.
		 */
		constinit u64 next_id = 0;
	}

	LifetimeScopeTree::LifetimeScopeTree(): scopes(), root(generateRootScope()) {}

	ScopeRef LifetimeScopeTree::newScope(ScopeRef parent) {
		scopes.emplaceBack(parent, parent->depth + 1, next_id++);
		return scopes.last();
	}

	LifetimeScopeTree::ScopeRef LifetimeScopeTree::generateRootScope() {
		scopes.emplaceBack(nullptr, 0, next_id++);
		return scopes.last();
	}
}
