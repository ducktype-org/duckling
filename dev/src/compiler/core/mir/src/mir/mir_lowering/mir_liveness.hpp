
#pragma once

#include <mir/mir_lowering/mir_destructors.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <diagnostic/stable_position.hpp>

#include <algorithm>
#include <vector>

namespace compiler::mir {
	/**
	 * @brief Move state of a local at a program point.
	 *
	 * - @ref Alive      — definitely initialized and not moved out of; safe to use.
	 * - @ref Moved      — definitely moved out of on every path reaching this point; using it is a
	 *                     use-after-move.
	 * - @ref MaybeMoved — moved out of on some (but not all) incoming paths; using it is reported
	 *                     as a use of a possibly-moved value.
	 *
	 * A local absent from the map is treated as uninitialized (not yet constructed on this path).
	 */
	enum class MoveStatus { Alive, Moved, MaybeMoved };

	/**
	 * @brief Move state of a single local at a program point.
	 */
	struct MoveState {
		/**
		 * @brief Reference to the local.
		 */
		MIRLocalRef local;

		/**
		 * @brief Actual move status of the local.
		 *
		 */
		MoveStatus status;

		/**
		 * @brief The list of move instructions that "reach" this point
		 * (reaching-definitions style). This lets diagnostics point at every place a value was
		 * moved, e.g. when both branches of an if/else move the same local.
		 *
		 * It is meaningful only for  @ref Status::Moved and @ref Status::MaybeMoved.
		 * Positions may be missing for compiler-generated instructions.
		 */
		std::vector<dia::StablePosition> move_sites;

		/**
		 * @brief Two states are equal when they have the same status and the same set of move
		 * sites. The comparison of move sites is order-independent.
		 */
		bool operator==(const MoveState& other) const {
			if (status != other.status) return false;
			if (move_sites.size() != other.move_sites.size()) return false;
			// Sizes are equal, so containment one way is enough.
			return std::ranges::all_of(move_sites, [&](const auto& pos) {
				return std::ranges::contains(other.move_sites, pos);
			});
		}
	};

	struct MoveStateData;

	/**
	 * @brief Move state of all tracked locals at a program point, together with the scope of the
	 * last instruction that was applied to it.
	 */
	class LocalMoveStateMap final {
		base::HashMap<LocalID, MoveState> map;
		/**
		 * @brief Scope of the last instruction passed to @ref updateMoveStateMapByInstr, or none
		 * when no instruction has been applied yet.
		 */
		base::Optional<ScopeRef> prev_instr_scope;

		/**
		 * @brief Mark @p local as alive with no reaching move sites.
		 */
		void markAlive(MIRLocalRef local) {
			map.insertOrAssign(
				local->id, MoveState{ .local = local, .status = MoveStatus::Alive, .move_sites = {} }
			);
		}

		/**
		 * @brief Merge two maps coming from two control-flow paths.
		 *
		 * @p into is the scope the joined program point lives in,
		 * i.e. the beginning scope of the successor block (where the maps are
		 * predecessors).
		 */
		static LocalMoveStateMap join(
			const LocalMoveStateMap& a, const LocalMoveStateMap& b, ScopeRef into
		);

		/**
		 * @brief Whether both maps hold the same data-flow state, i.e. the same move state for
		 * the same locals.
		 *
		 * @note This is deliberately not an `operator==`: @ref prev_instr_scope is not part of
		 * the comparison, as it is not a part of the data-flow state, so two maps that compare
		 * the same here are not interchangeable.
		 */
		bool hasSameDataFlowState(const LocalMoveStateMap& other) const;

	public:
		/**
		 * @brief Move state of @p local, or none when it is uninitialized at this point.
		 */
		base::Optional<CRef<MoveState>> stateOf(LocalID local) const { return map.atMaybe(local); }

		/**
		 * @brief Given an instruction, update the move-state info by the new instruction.
		 * For example the instruction that moves a variable updates the map, so that the
		 * new state for the variable is moved.
		 */
		void updateMoveStateMapByInstr(
			const Instruction& instr, const LocalsByScopeMap& locals_by_scope
		);

		void debugPrint(std::ostream& out) const;

		friend MoveStateData;
	};

	struct MoveStateData {
		/**
		 * @brief Move state of the local variables at the beginning of each block.
		 */
		base::HashMap<BlockID, LocalMoveStateMap> block_in_move_state;

		/**
		 * @brief Calculate the MoveStateData of the function.
		 */
		static MoveStateData calculateGlobalInMoveStateMap(
			const Function&                                     fun,
			const base::HashMap<BlockID, std::vector<BlockID>>& block_predecessors,
			const LocalsByScopeMap&                             locals_by_scope
		);
		void debugPrint(std::ostream& out);
	};

}
