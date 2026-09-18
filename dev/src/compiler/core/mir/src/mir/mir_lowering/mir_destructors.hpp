#pragma once

#include <mir/mir_structure/mir_lifetime_scope.hpp>
#include <mir/mir_structure/mir_local_ref.hpp>

#include <base/collections/maps.hpp>

#include <vector>

namespace compiler::mir {

	struct Function;

	/**
	 * @brief Helper lowest common ancestor of @p a and @p b
	 */
	ScopeRef lca(ScopeRef a, ScopeRef b);

	using LocalsByScopeMap = base::HashMap<ScopeRef, std::vector<MIRLocalRef>, ScopeRefHash>;

	/**
	 * @brief Returns list of scopes that lifetime ends between two consecutive instruction,
	 * first from @p begin scope, second from @p end scope.
	 * In general its the list of scopes between @p begin and lca(begin, end).
	 *
	 * This is a general implementation that always works.
	 * In the future we should find some invariant about two consecutive scopes
	 * that we validate.
	 */
	std::vector<ScopeRef> getEndingScopes(ScopeRef begin, ScopeRef end);

	/**
	 * @brief Get the starting scopes between two consecutive instructions.
	 *
	 * The order of scopes is the same as the the order of variables they
	 * would create (i.e. the first scope in the list is the one that creates variables that are
	 * created first).
	 */
	std::vector<ScopeRef> getStartingScopes(ScopeRef begin, ScopeRef end);

	/**
	 * @brief Check whether @p local is still in scope at @p scope, i.e. whether the lifetime
	 * scope of @p local is an ancestor of (or equal to) @p scope.
	 *
	 * This is a pure query on the scope tree, it does not modify @p local.
	 */
	bool isAliveInScope(MIRLocalRef local, ScopeRef scope);
}
