#pragma once

#include "mir_liveness.hpp"

#include <mir/mir_structure/mir_lifetime_scope.hpp>
#include <mir/mir_structure/mir_local_ref.hpp>

#include <query_framework/context/context_fd.hpp>

namespace compiler::mir {

	/**
	 * @brief Helper lowest common ancestor of @p a and @p b
	 */
	ScopeRef lca(ScopeRef a, ScopeRef b);


	struct Function;

	using LocalsByScopeMap = base::HashMap<ScopeRef, std::vector<MIRLocalRef>, ScopeRefHash>;

	struct LifetimePassArgs {
		LocalsByScopeMap                             locals_by_scope;
		MoveStateData                                 move_states;
		base::HashMap<BlockID, std::vector<BlockID>> block_predecessors;
	};

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
	 * @brief Add "MOVE" flags when copied from temporaries in instructions
	 * and the value is last-used.
	 */
	class AddMoves final : public LifetimePass {
	public:
		void run(query::Context&, Function&, const LifetimePassArgs&) final;
	};

	/**
	 * @brief Use after move and use uninitialized.
	 */
	class InvalidUseCheck final: public LifetimePass {
	public:
		void run(query::Context&, Function&, const LifetimePassArgs&) final;
	};

	/**
	 * @brief Perform a pass of MIR, that adds destructor calls
	 * based on instruction lifetime-scopes and move-state information.
	 */
	class AddDestructorsPass final: public LifetimePass {
	public:
		void run(query::Context&, Function&, const LifetimePassArgs&) final;
	};

	/**
	 * @brief Perform a pass of MIR that inserts a destructor call for the value being overwritten
	 * by an assignment.
	 *
	 * For every instruction that writes to a place (its `output`), a `Destruct` of the old value is
	 * inserted immediately before it, but only when all of the following hold:
	 * - the instruction does not initialize that place in this step (no `Construct` flag) — a fresh
	 * initialization has no previous value to destroy. A `Reinit` (whole-local reassignment) IS an
	 * override, so it does get a destructor;
	 * - the place's base local is `Alive` at that point (there is a live value to destroy);
	 * - the place's type has a non-trivial destructor.
	 *
	 * The inserted destructor shares the scope of the assignment instruction. It reuses the same
	 * `MIRPlace`, so its projections (field/index) are evaluated only once.
	 */
	class AddAssignmentDestructorsPass final: public LifetimePass {
	public:
		void run(query::Context&, Function&, const LifetimePassArgs&) final;
	};

	/**
	 * @brief Performs a pass of MIR, that adds `ScopeStart` and `ScopeEnd` flags to
	 * instructions based on scopes of variables. These are not lifetimes, but the places
	 * where we should allocate and de-allocate memory for variables,
	 * used by the DVM backend.
	 *
	 * The example of the `ScopeStart` and `Construct` being in different instructions:
	 * ```
	 * tmp = expression result (scope start x), (scope start tmp)
	 * x = tmp (construct x) (scope end tmp)
	 * ```
	 * the `scope start x` has to be before the `scope start tmp`, because the `tmp `is
	 * destroyed before the `x` and we want the start/end pairs to be like a stack.
	 * The lifetime starts with the `construct`, but we have the memory allocated already at the
	 * `scope start`.
	 *
	 * @see src/compiler/core/mir/init_deinit_dvm.md for more info.
	 */
	class AddScopeFlagsPass final: public LifetimePass {
	public:
		void run(query::Context&, Function&, const LifetimePassArgs&) final;
	};

	Function runAllLifetimePasses(query::Context& ctx, Function function);
}
