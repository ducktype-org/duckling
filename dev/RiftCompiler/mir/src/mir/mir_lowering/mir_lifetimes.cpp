#include "mir_lifetimes.hpp"
#include "../mir_structure/mir_structure.hpp"

namespace compiler::mir {
	Function addDestructors(query::Context& ctx, Function function) {
		// hmmm... BlockID
		// for now block order is important..

		std::vector<Block> new_blocks;

		for (auto& block: function.blocks) {
			std::vector<Instruction> new_instructions;
			new_instructions.reserve(block.instructions.size());
			
			for (auto& instr: block.instructions) {
				new_instructions.push_back(instr);
				
				// get successors...
				// get ending scopes
				// get live variables..
				// destruct live variables that are ended here
			}

			new_blocks.push_back({ block.id, std::move(new_instructions), block.terminator });
		}


		function.blocks = std::move(new_blocks);
		return function;
	}
}
