
#pragma once

#include <diagnostic_interactive/stable_position.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <vector>

namespace compiler::mir {
	/**
	 * @brief Liveness state of a local at a program point.
	 *
	 * - @ref Alive      — definitely initialized and not moved out of; safe to use.
	 * - @ref Moved      — definitely moved out of on every path reaching this point; using it is a
	 *                     use-after-move.
	 * - @ref MaybeMoved — moved out of on some (but not all) incoming paths; using it is reported
	 *                     as a use of a possibly-moved value.
	 *
	 * A local absent from the map is treated as uninitialized (not yet constructed on this path).
	 */
	enum class LivenessStatus { Alive, Moved, MaybeMoved };

	/**
	 * @brief Liveness state of a single local at a program point.
	 */
	struct LivenessState {
		/**
		 * @brief Status of the local.
		 */
		LivenessStatus kind;
		/**
		 * @brief The list of move instructions that "reach" this point
		 * (reaching-definitions style). This lets diagnostics point at every place a value was
		 * moved, e.g. when both branches of an if/else move the same local.
		 *
		 * It is meaningful only for  @ref Status::Moved and @ref Status::MaybeMoved.
		 * Positions may be missing for compiler-generated instructions.
		 */
		std::vector<dia_int::StablePosition> move_sites;
	};

	using LocalLivenessMap = base::HashMap<LocalID, LivenessState>;

	struct LivenessData {
		/**
		 * @brief Liveness state of the local variables at the beginning of each block.
		 */
		base::HashMap<BlockID, LocalLivenessMap> block_in_liveness;

		void debugPrint(std::ostream& out);
	};

	/**
	 * @brief Calculate the LivenessData of the function.
	 */
	LivenessData calculateGlobalInLivenessMap(
		const Function& fun, const base::HashMap<BlockID, std::vector<BlockID>>& block_predecessors
	);

	/**
	 * @brief Given an instruction, update liveness info map by the new instruction.
	 * For example the instruction that moves a variable updates the map, so that the
	 * new state for the variable is moved.
	 */
	void updateLivenessMapByInstr(LocalLivenessMap& map, const Instruction& instr);
}
