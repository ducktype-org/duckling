#include "mir_lifetime_scope.hpp"

#include <atomic>

namespace compiler::mir {

	namespace {
		/**
		 * Simple counter for generating unique scope IDs.
		 * Atomic to allow concurrent LifetimeScopeTree creation from multiple worker threads.
		 */
		constinit std::atomic<u64> next_id = 0;
	}

	LifetimeScopeTree::LifetimeScopeTree(): scopes(), root(generateRootScope()) {}

	ScopeRef LifetimeScopeTree::newScope(ScopeRef parent) {
		scopes.emplaceBack(
			parent, parent->depth + 1, next_id.fetch_add(1, std::memory_order_relaxed)
		);
		return scopes.last();
	}

	LifetimeScopeTree::ScopeRef LifetimeScopeTree::generateRootScope() {
		scopes.emplaceBack(nullptr, 0, next_id.fetch_add(1, std::memory_order_relaxed));
		return scopes.last();
	}
}
