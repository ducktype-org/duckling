#include "mir_lifetimes.hpp"
#include "../mir_structure/mir_structure.hpp"

namespace compiler::mir {

	// hmm... we might want to replace it with some kind of explicit graph creation
	// and walking.

	/**
	 * @brief Lowest common ancestor of @p a and @p b
	 * 
	 * @param a 
	 * @param b 
	 * @return helios::ScopeID 
	 */
	helios::ScopeID lca(helios::ScopeID a, helios::ScopeID b) {
		// @TODO: optimize it

		auto depth_a = helios::scopeDepth(a);
		auto depth_b = helios::scopeDepth(b);

		while (depth_a > depth_b) {
			a = helios::parent(a).value();
			depth_a--;
		}
		while (depth_b > depth_a) {
			b = helios::parent(b).value();
			depth_b--;
		}
		while (a != b) {
			a = helios::parent(a).value();
			b = helios::parent(b).value();
		}

		return a;
	}

	helios::ScopeID beginScope(const Block& block) {
		if (block.instructions.empty()) {
			return block.terminator.scope;
		}
		else {
			return block.instructions.at(0).scope;
		}
	}

	std::vector<helios::ScopeID> getEndingScopes(helios::ScopeID begin, helios::ScopeID end) {
		std::vector<helios::ScopeID> result;

		auto ancestor = lca(begin, end);
		while (begin != ancestor) {
			result.push_back(begin);
			begin = helios::parent(begin).value();
		}

		return result;
	}


	Function addDestructors(query::Context& ctx, Function function) {
		// hmmm... BlockID
		// for now block order is important..

		std::vector<Block> new_blocks;

		std::map<helios::ScopeID, std::vector<LocalRef>> locals_by_scope;
		for (auto& local: function.local_list) {
			locals_by_scope[local->lifetime_scope].emplace_back(local.ref());
		}

		for (auto& block: function.blocks) {
			std::vector<Instruction> new_instructions;
			new_instructions.reserve(block.instructions.size());
			
			for (u64 i = 0; i < block.instructions.size(); i++) {
				const auto& instr = block.instructions.at(i);
				const auto& next_instr = block.instructions.at(i + 1);
				
				new_instructions.push_back(instr);
				
				auto ending_scopes = getEndingScopes(instr.scope, next_instr.scope);

				for (auto scope: ending_scopes) {
					auto& locals = locals_by_scope[scope];
					for (auto& local: locals) {
						new_instructions.push_back(Instruction{
							Operation::DestructIf,
							{},
							{ local },
							{ OperationFlag{OperationFlag::Flag::Destruct, local } },
							instr.scope
						});
					}
				}


				// No live analysis here, since it is quite complex.
				// It might be best to do it in a separate pass, when
				// we already have destructors added.


			}

			// next handle terminator (TODO TODO to be done....):

			new_blocks.push_back({ block.id, std::move(new_instructions), block.terminator });
		}


		function.blocks = std::move(new_blocks);
		return function;
	}
}
