/**
 * This file holds structure defining mir LifetimeScope,
 * and mir LifetimeScopeTree. These are the structures
 * that then MIR uses to handle lifetime scopes of variables and instructions.
 * MIR scopes are in many ways similar to typical scopes in the program, differing mostly in some
 * details and corner cases.
 */

#pragma once

#include <base/collections/stable_container.hpp>
#include <base/pointers/ref.hpp>

namespace compiler::mir {

	/**
	 * A simple tree-like structure that holds MIR lifetime scopes.
	 * It allows for new scope creation in imperative manner.
	 * @note It doesn't have any special semantical properties, it is literally just a tree
	 * used by MIR for lifetime scopes.
	 *
	 * Each MIR function will creates its own LifetimeScopeTree during its creation.
	 */
	struct LifetimeScopeTree final {
		/**
		 * MIR Lifetime scope.
		 */
		struct LifetimeScope final {
			MCRef<LifetimeScope> parent;
			u64                  depth;

			/**
			 * Unique runtime id, for mapping, comparision, etc.
			 */
			u64 id;

			bool operator==(const LifetimeScope& other) const { return id == other.id; }
		};

		using ScopeRef = CRef<LifetimeScopeTree::LifetimeScope>;

	private:
		/**
		 * Storage of all scopes within given tree.
		 * It is a stable vector for convenience of using it simple references to the scopes.
		 * @note We can refactor it to more competitive-programming like approach in the future
		 * if it will be needed for performance.
		 */
		base::StableVector<const LifetimeScope> scopes;

		/**
		 * Creates a root scope for this tree.
		 */
		ScopeRef generateRootScope();

	public:
		/**
		 * Root of the scope tree, created at tree creation, with depth 0, and no parent.
		 */
		ScopeRef root;

		LifetimeScopeTree();

		/**
		 * Creates a new scope, with given parent.
		 */
		ScopeRef newScope(ScopeRef parent);
	};

	using ScopeRef = LifetimeScopeTree::ScopeRef;

	struct ScopeRefHash final {
		size_t operator()(const ScopeRef scope) const noexcept {
			return std::hash<u64>{}(scope->id);
		}
	};
}
