#include "../mir_structure/mir_structure.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>

#include <algorithm>
#include <unordered_set>
#include <variant>

namespace compiler::mir {
	bool validateMoves(const Function& fun) {
		using LocalSet      = std::unordered_set<LocalID>;
		using BlockLocalSet = base::HashMap<BlockID, LocalSet>;

		BlockLocalSet
			moved_variables;  // Variables moved in Block, they can't be used after this Block,
		BlockLocalSet used_variables;  // variables which must be valid, at the begining of Block.

		for (const auto& block: fun.blocks) {  // Fill with blocks.
			moved_variables.maybePut(block.key, LocalSet());
			used_variables.maybePut(block.key, LocalSet());
		}
		base::HashMap<LocalID, BlockID>
			construction_block;  // For each Local store where it is constructed.

		// Anlyse each block independently.
		for (const auto& block: fun.blocks) {
			auto process_instruction = [&](Instruction instr) {
				if (instr.operation == Operation::Destruct
				    || instr.operation == Operation::DestructIf)

					return true;  // LIR decides whether destruction should be performed.

				// Firstly list all arguments - They must be valid.
				for (const auto& arg: instr.arguments) {
					if (arg.isLocal()) {
						used_variables[block.key].insert(
							arg.get<MIRPlace>().getBase<MIRLocalRef>()->id
						);

						if (moved_variables[block.key].contains(
								arg.get<MIRPlace>().getBase<MIRLocalRef>()->id
							))
							return false;  // It is already moved.
					}
				}

				// Output can't be local, already moved, variable.
				if (instr.output.has_value() && instr.output.value().isLocal()) {
					used_variables[block.key].insert(instr.output.value().getBase<MIRLocalRef>()->id
					);

					if (moved_variables[block.key].contains(
							instr.output.value().getBase<MIRLocalRef>()->id
						))
						return false;  // It is already moved.
				}

				for (const auto& flag: instr.flags) {
					if (flag.flag == OperationFlag::Flag::Move) {
						// @note Now we assume that variable moved in instruction, must be
						// its argument and appear exactly one time there. It can't be
						// output of instruction. It may change in the future.

						if (moved_variables[block.key].contains(flag.local->id))
							return false;  // Already moved.

						moved_variables[block.key].insert(flag.local->id);

						if (std::ranges::count_if(
								instr.arguments,
								[&](const auto& arg) {
									return arg.isLocal()
							            && arg.template get<MIRPlace>()
							                       .template getBase<MIRLocalRef>()
							                       ->id
							                   == flag.local->id;
								}
							)
						    != 1)
							return false;  // Used 0 or 2 or more times as argument.

						if (instr.output.has_value() && instr.output.value().isLocal()
						    && instr.output.value().getBase<MIRLocalRef>()->id == flag.local->id)
							return false;  // Moved local used as output.
					}
					if (flag.flag == OperationFlag::Flag::Construct) {
						// Assume constructors are valid (every use is after construct).
						construction_block.maybePut(flag.local->id, block.key);
					}
					// Ommit destruct flag - LIR will handle it.
				}
				return true;
			};

			for (const auto& instruction: block.value.instructions)
				if (!process_instruction(instruction)) return false;

			if (!process_instruction(block.value.terminator)) return false;
		}

		// Now we perform global analysys.

		// For each variable start DFS starting in block of its construction. Look for any use after
		// move, visit all achievable blocks, except for starting one. Each block can be visited in
		// two states: variable can be used and can't.


		constexpr int USABLE = 0, NOT_USABLE = 1;

		struct States final {
			bool state[2] = { false, false };
		};

		// It should be HashSet<BlockID, state>, but there is no hash.
		base::HashMap<BlockID, States> visited;  // with usable and not usable.

		// Insert all blocks.
		for (const auto& id: fun.block_order) visited.maybePut(id, States());

		for (const auto& local: construction_block) {
			for (const auto& id: fun.block_order)
				visited[id].state[USABLE] = false, visited[id].state[NOT_USABLE] = false;

			const auto& starting_block = local.value;

			auto visit
				= [&](this const auto& self, const BlockID& id, const int& cr_state) -> bool {
				visited[id].state[cr_state] = true;
				int next_state              = NOT_USABLE;
				if (cr_state == NOT_USABLE) {
					if (moved_variables[id].contains(local.key)
					    || used_variables[id].contains(local.key))
						return false;
					next_state = NOT_USABLE;
				} else {
					if (moved_variables[id].contains(local.key))
						next_state = NOT_USABLE;
					else
						next_state = USABLE;
				}

				for (const auto& next_block: getTerminatorSuccessors(fun.blocks[id].terminator)) {
					if (next_block != starting_block && !visited[next_block].state[next_state]) {
						if (!self(next_block, next_state)) return false;
					}
				}
				return true;
			};

			if (!visit(starting_block, USABLE)) return false;
		}

		return true;
	}

	base::OkBad validateFunction(const Function& fun) {
		return validateMoves(fun) ? base::OK : base::BAD;
	}

}
