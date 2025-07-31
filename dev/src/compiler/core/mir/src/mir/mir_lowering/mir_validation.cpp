#include "../mir_structure/mir_structure.hpp"

#include <base/maps.hpp>

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace compiler::mir {

	bool validateMoves(const Function& fun) {
		std::unordered_map<BlockID, base::Map<LocalID, u64>>
			moved_variables;  // Variables used in Block, they can't be used after this Block.
		std::unordered_map<BlockID, std::vector<LocalID>>
			used_variables;   // Variables which must be valid, at the begining of Block.

		std::unordered_map<BlockID, std::unordered_set<BlockID>>
			prev_blocks;  // Prev_block is transposed transition graph of function.

		for (const auto& block: fun.blocks) {

			auto process_instruction = [&](Instruction instr){
				for (const auto& flag: instr.flags){
					if (flag.flag == OperationFlag::Flag::Move) { // Move
						if (!moved_variables[block.first].contains(flag.local->id))
							moved_variables[block.first].emplace(flag.local->id, 0);
						++moved_variables[block.first][flag.local->id];
					}else{ // Construct or desctruct
						used_variables[block.first].push_back(flag.local);
					}
				}

				// Argument usage:
				

			};

			for (const auto& instruction: block.second->instructions) {
				for (const auto& flag: instruction.flags)
					if (flag.flag == OperationFlag::Flag::Move) {
						if (!moved_variables[block.first].contains(flag.local->id))
							moved_variables[block.first].emplace(flag.local->id, 0);
						++moved_variables[block.first][flag.local->id];
					}
			}  // Count moves in each block body.

			// And terminator.
			const auto& terminator = block.second->terminator;
			for (const auto& flag: terminator.flags) {
				if (flag.flag == OperationFlag::Flag::Move) {
					if (!moved_variables[block.first].contains(flag.local->id))
						moved_variables[block.first].emplace(flag.local->id, 0);
					++moved_variables[block.first][flag.local->id];
				}
			}

			for (const auto& next_block: getTerminatorSuccessors(terminator))
				prev_blocks[next_block].emplace(block.first);
		}

		std::unordered_set<BlockID> visited;
		std::vector<BlockID>        post_order;

		auto dfs1 = [&](this const auto& self,
		                const BlockID&   cr) -> void {  // visits graph, sets post-order
			visited.insert(cr);
			for (const auto& next_block: getTerminatorSuccessors(fun.blocks[cr].terminator))
				if (!visited.contains(next_block)) self(next_block);
			post_order.push_back(cr);
		};

		for (const auto& block: fun.block_order)
			if (!visited.contains(block)) dfs1(block);
		std::reverse(post_order.begin(), post_order.end());
		visited.clear();

		std::unordered_map<BlockID, u64> connected_component;
		u64                              current_connected = 0;

		auto dfs2 = [&](this const auto& self, const BlockID& cr)
			-> void {  // Mark all reachable, unvisited blocks with currect connected component id.
			visited.insert(cr);
			connected_component[cr] = current_connected;
			for (const auto& next_block: prev_blocks[cr])
				if (!visited.contains(next_block)) self(next_block);
		};

		for (const auto& block: post_order) {
			if (!visited.contains(block)) {
				dfs2(block);
				++current_connected;
			}
		}

		std::vector<base::Map<LocalID, u64>> moved_in_cycle(current_connected);
		std::vector<std::vector<u64>> edges(current_connected);  // compressed transposed graph
		// Each connected component must have 0 moves (or consists of exactly 1 node and 0 edges).

		// Set edges.
		for (const auto& source: prev_blocks) {
			for (const auto& target: source.second) {
				if (connected_component[source.first] != connected_component[target]) {
					edges[connected_component[target]].push_back(connected_component[source.first]);
				} else if (!moved_variables[source.first].empty()
				           || !moved_variables[target].empty()) {
					return false;  // There exists some cycle between source and target and there
					               // are moves in it.
				}
			}
		}

		//  Set moved in cycle.
		for (const auto& block: moved_variables) {
			for (const auto& variable: block.second) {
				if (!moved_in_cycle[connected_component[block.first]].contains(variable.first))
					moved_in_cycle[connected_component[block.first]].emplace(variable.first, 0);
				moved_in_cycle[connected_component[block.first]][variable.first] += variable.second;
				if (moved_in_cycle[connected_component[block.first]][variable.first] > 1)
					return false;
			}
		}

		// All cycles all valid on their own. Check whole graph.
		for (i64 i = current_connected - 1; i >= 0; --i) {
			for (auto incoming_branch: edges[i]) {
				for (auto moved: moved_in_cycle[incoming_branch])
					if (moved.second && moved_in_cycle[i].contains(moved.first)) return false;
			}

			for (auto incoming_branch: edges[i])
				for (auto moved: moved_in_cycle[incoming_branch])
					moved_in_cycle[i].emplace(moved.first, 1);
		}


		return true;
	}

	bool validateFunction(const Function& fun) { return validateMoves(fun); }

}
