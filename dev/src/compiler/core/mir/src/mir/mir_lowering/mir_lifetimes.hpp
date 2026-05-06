#pragma once

#include <mir/mir_structure/mir_lifetime_scope.hpp>

#include <query_framework/context/context_fd.hpp>

namespace compiler::mir {

	/**
	 * @brief Helper lowest common ancestor of @p a and @p b
	 */
	ScopeRef lca(ScopeRef a, ScopeRef b);


	struct Function;
	struct LifetimePassArgs;

	/**
	 * @brief Abstract class representing a pass that adds lifetime related instructions and flags
	 * to MIR.
	 */
	class LifetimePass {
	public:
		virtual ~LifetimePass() = default;

		virtual void run(query::Context&, Function&, const LifetimePassArgs&) = 0;
	};

	/**
	 * @brief Perform a pass of MIR, that adds destructor calls
	 * based on instruction lifetime-scopes.
	 *
	 * @important
	 * It is a mock implementation, and does not perform
	 * lifetime checks. This means that it will add destructors for all locals, even if they
	 * are not yet created. example:
	 * ```cpp
	 * fun foo() { return; var a: T; } // calls destructor on return
	 * ```
	 *
	 * @TODO: #2654 make it a final implementation, that takes liveness into account.
	 * Ideas:
	 *   * "add liveness" after adding destructors, and delete destructors that are not needed.
	 *   * make explicit cfg graph, and somehow walk it to find liveness ranges.
	 *   * somehow use lifetime_scopes to find liveness ranges.
	 */
	class AddDestructorsPass final: public LifetimePass {
	public:
		void run(query::Context&, Function&, const LifetimePassArgs&) final;
	};

	/**
	 * @brief Performs a pass of MIR, that adds `ScopeStart` and `ScopeEnd` flags to
	 * instructions based on scopes of variables. These are not lifetimes, but the places
	 * where we should allocate and de-allocate memory for variables,
	 * used by the DVM backend.
	 *
	 * @see src/compiler/core/mir/init_deinit_dvm.md for more info.
	 */
	class AddScopeFlagsPass final: public LifetimePass {
	public:
		void run(query::Context&, Function&, const LifetimePassArgs&) final;
	};

	Function runAllLifetimePasses(query::Context& ctx, Function function);
}
