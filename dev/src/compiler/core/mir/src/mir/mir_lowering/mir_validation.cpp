#include "../mir_structure/mir_structure.hpp"

#include <base/maps.hpp>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace compiler::mir {


	bool validateMoves(const Function& fun) {
		std::unordered_map<BlockID, std::unordered_set<LocalID>> moved_variables,
			used_variables;  // Variables moved in Block, they can't be used after this Block,
		                     // variables which must be valid, at the begining of Block.

		std::unordered_map<LocalID, BlockID> construction_block;

		for (const auto& block: fun.blocks) {
			auto process_instruction = [&](Instruction instr) {
				if (instr.operation == Operation::Destruct
				    || instr.operation == Operation::DestructIf)
					return true;  // Lir handles destructors.

				// Firstly list all arguments - They must be valid.
				for (const auto& arg: instr.arguments) {
					if (arg.isLocal()) {
						used_variables[block.first].insert(arg.get<LocalRef>()->id);
						if (moved_variables[block.first].contains(arg.get<LocalRef>()->id))
							return false;  // It is already moved.
					}
				}

				// Output can't be local, already moved, variable.
				if (instr.output.has_value()
				    && std::holds_alternative<LocalRef>(instr.output.value())) {
					used_variables[block.first].insert(std::get<LocalRef>(instr.output.value())->id);
					if (moved_variables[block.first].contains(
							std::get<LocalRef>(instr.output.value())->id
						))
						return false;  // It is already moved.
				}

				for (const auto& flag: instr.flags) {
					if (flag.flag == OperationFlag::Flag::Move) {
						// @note Now we assume that variable moved in instruction, must be its
						// argument and appear exactly one time there. It can't be output of
						// instruction. It may change in the future.

						if (moved_variables[block.first].contains(flag.local->id))
							return false;  // Already moved.

						moved_variables[block.first].insert(flag.local->id);

						if (std::ranges::count_if(
								instr.arguments,
								[&](const auto& arg) {
									return arg.isLocal()
							            && (arg.template get<LocalRef>()->id == flag.local->id);
								}
							)
						    != 1)
							return false;  // Used 0 or 2 or more times as argument.

						if (instr.output.has_value()
						    && std::holds_alternative<LocalRef>(instr.output.value())
						    && std::get<LocalRef>(instr.output.value())->id == flag.local->id)
							return false;  // Moved local used as output.
					}
					if (flag.flag == OperationFlag::Flag::Construct) {
						// Assume constructors are valid (every use is after construct).
						construction_block[flag.local->id] = block.first;
					}
					// Ommit destruct flag - LIR will handle it.
				}
				return true;
			};

			for (const auto& instruction: block.second->instructions)
				if (!process_instruction(instruction)) return false;

			if (!process_instruction(block.second->terminator)) return false;
		}

		// For each variable start DFS starting in block of its construction. Look for any use after
		// move, visit all achievable blocks, except for starting one. Each block can be visited in
		// two states: variable can be used and can't.

		std::unordered_map<BlockID, bool[2]> visited;  // with usable and not usable.
		const int                            usable = 0, not_usable = 1;

		for (const auto& local: construction_block) {
			for (const auto& id: fun.block_order)
				visited[id][usable] = false, visited[id][not_usable] = false;

			const auto& starting_block = local.second;

			auto visit = [&](this const auto& self, const BlockID& id, const int& state) -> bool {
				visited[id][state] = true;
				int next_state     = not_usable;
				if (state == not_usable) {
					if (moved_variables[id].contains(local.first)
					    || used_variables[id].contains(local.first))
						return false;
					next_state = not_usable;
				} else {
					if (moved_variables[id].contains(local.first))
						next_state = not_usable;
					else
						next_state = usable;
				}

				for (const auto& next_block: getTerminatorSuccessors(fun.blocks[id].terminator)) {
					if (next_block != starting_block && !visited[next_block][next_state]) {
						if (!self(next_block, next_state)) return false;
					}
				}
				return true;
			};

			if (!visit(starting_block, usable)) return false;
		}

		return true;
	}

	base::OkBad validateFunction(const Function& fun) {
		return validateMoves(fun) ? base::OK : base::BAD;
	}

}
