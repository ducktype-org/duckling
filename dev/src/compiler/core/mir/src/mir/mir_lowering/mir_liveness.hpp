
#pragma once

#include <diagnostic_interactive/stable_position.hpp>
#include <mir/mir_structure/mir_structure.hpp>

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
		 * @brief Status of the local.
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
		std::vector<dia_int::StablePosition> move_sites;

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

	using LocalMoveStateMap = base::HashMap<LocalID, MoveState>;

	struct MoveStateData {
		/**
		 * @brief Move state of the local variables at the beginning of each block.
		 */
		base::HashMap<BlockID, LocalMoveStateMap> block_in_move_state;

		void debugPrint(std::ostream& out);
	};

	/**
	 * @brief Calculate the MoveStateData of the function.
	 */
	MoveStateData calculateGlobalInMoveStateMap(
		const Function& fun, const base::HashMap<BlockID, std::vector<BlockID>>& block_predecessors
	);

	/**
	 * @brief Given an instruction, update move-state info map by the new instruction.
	 * For example the instruction that moves a variable updates the map, so that the
	 * new state for the variable is moved.
	 */
	void updateMoveStateMapByInstr(LocalMoveStateMap& map, const Instruction& instr);
}
