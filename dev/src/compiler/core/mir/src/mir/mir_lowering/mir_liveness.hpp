
#pragma once

#include "mir/mir_structure/mir_structure.hpp"

namespace compiler::mir {
	enum class Status { Alive, Moved, MaybeMoved };

	using LocalStatusMap = base::HashMap<LocalID, Status>;

	struct LivenessData {
		base::HashMap<BlockID, LocalStatusMap> block_in_liveness;
	};

	LivenessData calculateGlobalInLivenessStatus(const Function& fun, const base::HashMap<BlockID, std::vector<BlockID>>& block_predecessors);

	void updateLivenessMapByInstr(LocalStatusMap& map, const Instruction& instr);
}
