
#pragma once

#include <diagnostic_interactive/stable_position.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <vector>

namespace compiler::mir {
	enum class LivenessStatus { Alive, Moved, MaybeMoved };

	/**
	 * @brief Dataflow state of a single local at a program point.
	 *
	 * Besides the @ref Status kind it carries the set of move instructions that "reach" this point
	 * (reaching-definitions style). This lets diagnostics point at every place a value was moved,
	 * e.g. when both branches of an if/else move the same local.
	 *
	 * @p move_sites is meaningful only for @ref Status::Moved and @ref Status::MaybeMoved; it is
	 * empty for @ref Status::Alive. Positions may be missing for compiler-generated instructions,
	 * so a moved value can legitimately have an empty @p move_sites.
	 */
	struct LivenessState {
		LivenessStatus                       kind;
		std::vector<dia_int::StablePosition> move_sites;
	};

	using LocalLivenessMap = base::HashMap<LocalID, LivenessState>;

	struct LivenessData {
		base::HashMap<BlockID, LocalLivenessMap> block_in_liveness;

		void debugPrint(std::ostream& out);
	};

	LivenessData calculateGlobalInLivenessMap(
		const Function& fun, const base::HashMap<BlockID, std::vector<BlockID>>& block_predecessors
	);

	void updateLivenessMapByInstr(LocalLivenessMap& map, const Instruction& instr);
}
