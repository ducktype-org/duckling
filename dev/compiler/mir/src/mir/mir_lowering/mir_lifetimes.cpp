#include "mir_lifetimes.hpp"

#include "../mir_structure/mir_structure.hpp"

#include <helios/scopes/simple.hpp>

namespace compiler::mir {

	/**
	 * @brief Lowest common ancestor of @p a and @p b
	 *
	 * @param a
	 * @param b
	 * @return helios::ScopeID
	 */
	helios::ScopeID lca(helios::ScopeID a, helios::ScopeID b) {
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

	/**
	 * @brief Returns list of scopes that lifetime ends between two consecutive instruction,
	 * first from @p begin scope, second from @p end scope.
	 * In general its the list of scopes between @p begin and lca(begin, end).
	 *
	 * @todo this is a general implementation that always works.
	 * In the future we should find some invariant about two consecutive scopes
	 * that we validate.
	 *
	 * @param begin
	 * @param end
	 * @return std::vector<helios::ScopeID>
	 */
	std::vector<helios::ScopeID> getEndingScopes(helios::ScopeID begin, helios::ScopeID end) {
		std::vector<helios::ScopeID> result;

		auto ancestor = lca(begin, end);
		while (begin != ancestor) {
			result.push_back(begin);
			begin = helios::parent(begin).value();
		}

		return result;
	}

	Function addDestructors(query::Context&, Function function) {
		// Idea of implementation: for each block we iterate over instructions
		// and add destructors after each instruction (often 0 of them),
		// based on scopes that ends there.
		// context is unused, but left since it might be useful in the future.
		// preserving block order is important, because of how MIR BlockIDs works

		std::map<helios::ScopeID, std::vector<LocalRef>> locals_by_scope;
		for (auto& local: function.local_list)
			locals_by_scope[local->lifetime_scope].emplace_back(local.ref());

		// No lifetime analysis here, since it is quite complex.
		// See doc-comment of this function for details.

		std::vector<std::vector<Instruction>> new_blocks_instructions;
		for (auto& block_id: function.block_order) {
			// each block is considered independently
			auto& block = function.blocks[block_id];

			std::vector<Instruction> new_instructions;
			new_instructions.reserve(block.instructions.size());

			// lambdas used just to not duplicate code:
			auto add_destructor = [&](helios::ScopeID instr_scope, LocalRef local) {
				new_instructions.push_back(Instruction{
					Operation::DestructIf,
					{},
					{ local },
					{ OperationFlag{ OperationFlag::Flag::Destruct, local } },
					instr_scope });
			};
			auto add_destructors = [&](const auto& ending_scopes, helios::ScopeID instr_scope) {
				for (auto scope: ending_scopes) {
					auto& locals = locals_by_scope[scope];
					for (auto& local: locals) add_destructor(instr_scope, local);
				}
			};

			for (u64 i = 0; i < block.instructions.size(); i++) {
				const auto& instr      = block.instructions.at(i);
				const auto& next_instr = i < block.instructions.size() - 1
				                           ? block.instructions.at(i + 1)
				                           : block.terminator;

				new_instructions.push_back(instr);

				auto ending_scopes = getEndingScopes(instr.scope, next_instr.scope);

				add_destructors(ending_scopes, instr.scope);
			}

			// we handle terminator in special way,
			// because its successors are a set, not a single object:

			auto& terminator = block.terminator;
			auto  successors = getTerminatorSuccessors(terminator);
			if (successors.empty()) {
				// the function ends
				auto ending_scopes = getEndingScopes(terminator.scope, function.top_lifetime_scope);
				add_destructors(ending_scopes, terminator.scope);
			} else {
				base::Optional<std::vector<helios::ScopeID>> ending_scopes;

				// we have to validate here that each path has the same ending scopes
				// @todo there are two possible futures:
				// * It will remain a valid assumption (intuitively it should, but some weird cases
				// might break it).
				//   Assume it is, and ensure it in MIR-Lowering
				// * It will not be a valid assumption, and we will have to change this
				// implementation. This hole for is just for this validation. Maybe we should have
				// some conditional compilation here based on debug/release modes
				for (auto succ: successors) {
					auto succ_ending_scopes
						= getEndingScopes(terminator.scope, function.blocks[succ].beginScope());

					if (ending_scopes.has_value()) {
						// we have to validate that all paths have the same ending scopes
						// otherwise this implementation is incorrect
						CORE_ASSERT(ending_scopes == succ_ending_scopes, "Different ending scopes");
					} else {
						ending_scopes.emplace(std::move(succ_ending_scopes));
					}
				}

				add_destructors(ending_scopes.value(), terminator.scope);
			}

			new_blocks_instructions.push_back(std::move(new_instructions));
		}

		for (usize i = 0; i < function.block_order.size(); i++) {
			auto block_id                          = function.block_order[i];
			function.blocks[block_id].instructions = std::move(new_blocks_instructions[i]);
		}

		return function;
	}
}
