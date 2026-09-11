#include "mir_destructors.hpp"

#include "../mir_structure/mir_structure.hpp"
#include "mir_lifetimes.hpp"
#include "mir_liveness.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/queries/types.hpp>

#include <algorithm>

namespace compiler::mir {

	ScopeRef lca(ScopeRef a, ScopeRef b) {
		auto depth_a = a->depth;
		auto depth_b = b->depth;

		while (depth_a > depth_b) {
			a = a->parent.toOpt().value();
			depth_a--;
		}
		while (depth_b > depth_a) {
			b = b->parent.toOpt().value();
			depth_b--;
		}
		while (a != b) {
			a = a->parent.toOpt().value();
			b = b->parent.toOpt().value();
		}

		return a;
	}

	std::vector<ScopeRef> getEndingScopes(ScopeRef begin, ScopeRef end) {
		std::vector<ScopeRef> result;

		auto ancestor = lca(begin, end);
		while (begin != ancestor) {
			result.push_back(begin);
			begin = begin->parent.toOpt().value();
		}

		return result;
	}

	std::vector<ScopeRef> getStartingScopes(ScopeRef begin, ScopeRef end) {
		return getEndingScopes(end, begin) | std::views::reverse | std::ranges::to<std::vector>();
	}

	bool isAliveInScope(MIRLocalRef local, ScopeRef scope) {
		return lca(*local->scope, scope) == *local->scope;
	}

	namespace {
		/**
		 * @brief If @p instr overwrites a live, non-trivially-destructible place that it does not
		 * initialize, build the `Destruct` of the old value to run before it. Otherwise returns none.
		 */
		base::Optional<Instruction> assignmentDestructorFor(
			query::Context& ctx, const Instruction& instr, const LocalMoveStateMap& move_states
		) {
			// The instruction must write to a place.
			if_opt_none(instr.output) return {};
			const MIRPlace& target = instr.output.value();

			base::Optional<CRef<MoveState>> local_move_state;

			if (target.isLocal()) {
				const MIRLocalRef base = target.getBase<MIRLocalRef>();

				if (base->lifetime_flags.contains(LifetimeFlag::NoDestructor)) return {};

				local_move_state = move_states.stateOf(base->id);
				// If variable is uninitialized or moved we don't insert destructor.
				if (local_move_state.empty()
				    || local_move_state.value()->status == MoveStatus::Moved)
					return {};

				for (const auto& flag: instr.flags) {
					if (flag.flag == OperationFlag::Flag::Construct) return {};
					// If we move this value in this instruction, and at the same time we override
					// it, we don't have to call destructor `a = call f(c, b, move a)`
					if (flag.flag == OperationFlag::Flag::Move && flag.local->id == base->id)
						return {};
				}
			}

			// The place's type must have a non-trivial destructor.
			auto destruct_sym_opt = helios::getTypeDestructor(ctx, target.type);
			if_opt_none(destruct_sym_opt) return {};

			Operation op = Operation::Destruct;
			if (local_move_state && local_move_state.value()->status == MoveStatus::MaybeMoved)
				op = Operation::DestructIf;

			return Instruction{
				op, {}, { MIRFunctionLiteral{ destruct_sym_opt.value() }, target }, {}, instr.scope,
			};
		}
	}

	void AddDestructorsPass::run(
		query::Context& ctx, Function& function, const LifetimePassArgs& args
	) {
		// Idea of implementation:
		// For each block we iterate over instructions and add destructors after each instruction
		// (often 0 of them), based on scopes that ends there.
		//
		// The pass is divided into two steps:
		// 1) Iterate through all instructions in the block. If moving from one instruction to the
		// 	  next crosses a scope boundary, we insert the necessary destructors between them.
		// 2) Handling terminators is more complex because they can have multiple successors with
		//	  different ending scopes. The general logic is to insert a new intermediate block for
		// 	  every path that crosses a scope boundary. This block contains only the destructors
		//    specific to that path and a `Jump` to the actual destination.
		//
		// OPT: Additionally, a simple optimization to not create a lot of blocks and insert
		// destructors into the current block in simple cases (like if-else with no ending scopes
		// inside) is implemented. For more info look at the big comment below.
		//
		// Context is unused, but left since it might be useful in the future.
		// No lifetime analysis here, since it is quite complex.
		// See doc-comment of this function for details.

		auto&                                 locals_by_scope = args.locals_by_scope;
		std::vector<std::vector<Instruction>> new_blocks_instructions;
		std::vector<BlockID>                  new_blocks_order;

		// Util for getting a crispy fresh MIR block.
		u64  next_id_val      = function.blocks.size() + 1;
		auto get_new_block_id = [&]() {
			while (function.blocks.contains(BlockID(next_id_val))) next_id_val++;
			return BlockID(next_id_val++);
		};

		// lambdas used just to not duplicate code:
		auto add_destructor_to_instr_vec = [&](ScopeRef                  instr_scope,
		                                       MIRLocalRef               local,
		                                       std::vector<Instruction>& out_instructions,
		                                       const LocalMoveStateMap&  move_states) {
			if (local->lifetime_flags.contains(LifetimeFlag::NoDestructor)) return;

			// Move-state analysis
			auto local_move_state = move_states.stateOf(local->id);
			if (local_move_state.empty()) return;  // Empty means that the local is uninitialized
			if (local_move_state.value()->status == MoveStatus::Moved)
				return;                            // When moved we also do not create destructor

			auto destruct_sym_opt = helios::getTypeDestructor(ctx, local->type);
			if_opt_none(destruct_sym_opt) return;
			auto destructor_symbol = destruct_sym_opt.value();

			Operation op = local_move_state.value()->status == MoveStatus::Alive
			                 ? Operation::Destruct
			                 : Operation::DestructIf;

			out_instructions.push_back(Instruction{
				op,
				{},
				{ MIRFunctionLiteral{ destructor_symbol }, local },
				{ flagDestruct(local) },
				instr_scope,
			});
		};

		auto add_destructors_to_instr_vec = [&](const auto&               ending_scopes,
		                                        std::vector<Instruction>& out_instructions,
		                                        const LocalMoveStateMap&  move_states) {
			for (const auto& scope: ending_scopes) {
				if (not locals_by_scope.contains(scope)) continue;
				auto& locals = locals_by_scope.at(scope);
				// Add destructors in reverse order.
				for (auto& local: locals | std::views::reverse)
					add_destructor_to_instr_vec(scope, local, out_instructions, move_states);
			}
		};


		for (auto& block_id: function.block_order) {
			// Add THIS block to our new block order.
			new_blocks_order.push_back(block_id);

			// each block is considered independently
			auto& block = function.blocks[block_id];

			// Move-state is only computed for blocks reachable from the entry. Unreachable blocks
			// are still present here (they get pruned later by eliminateUnreachable), so default to
			// an empty map for them instead of crashing.
			LocalMoveStateMap move_state_info;
			if (auto found = args.move_states.block_in_move_state.atMaybe(block_id))
				move_state_info = *found.value();

			std::vector<Instruction> new_instructions;
			new_instructions.reserve(block.instructions.size());


			for (u64 i = 0; i < block.instructions.size(); i++) {
				const auto& instr      = block.instructions.at(i);
				const auto& next_instr = i < block.instructions.size() - 1
				                           ? block.instructions.at(i + 1)
				                           : block.terminator;

				new_instructions.push_back(instr);
				move_state_info.updateMoveStateMapByInstr(instr, args.locals_by_scope);

				auto ending_scopes = getEndingScopes(instr.scope, next_instr.scope);

				add_destructors_to_instr_vec(ending_scopes, new_instructions, move_state_info);
			}

			// We handle terminator in special way, because its successors are a set, not a single
			// object.
			// We also assume that the block terminator never creates or moves any local variable.
			auto& terminator = block.terminator;
			auto  successors = getTerminatorSuccessors(terminator);
			if (successors.empty()) {
				// the function ends
				auto ending_scopes
					= getEndingScopes(terminator.scope, function.lifetime_scope_tree.root);
				add_destructors_to_instr_vec(ending_scopes, new_instructions, move_state_info);
			} else {
				// Here we handle situations where a branch may cause two different sets of
				// destructors being performed. For example breaking from a loop etc.
				base::Optional<std::vector<ScopeRef>>         first_path_ending_scopes;
				bool                                          paths_identical  = true;
				bool                                          boundary_crossed = false;
				base::HashMap<BlockID, std::vector<ScopeRef>> ending_scopes_per_succ;

				for (auto succ: successors) {
					auto succ_begin_scope   = function.blocks[succ].beginScope();
					auto succ_ending_scopes = getEndingScopes(terminator.scope, succ_begin_scope);

					if (!succ_ending_scopes.empty()) boundary_crossed = true;

					// Opt: Check if all path lead to the same ending scope.
					if (!first_path_ending_scopes.has_value())
						first_path_ending_scopes = succ_ending_scopes;
					else if (*first_path_ending_scopes != succ_ending_scopes)
						paths_identical = false;
					ending_scopes_per_succ.put(succ, std::move(succ_ending_scopes));
				}

				// Opt: Appending destructors directly to the current block, without creating an
				// intermediate is only safe when:
				// - all paths have the ending scopes
				// - either no boundary is crossed at all, or the only scope that ends here is the
				//   terminator's scope. The first element of `getEndingScopes(terminator.scope)`
				//   is always terminator.scope itself, so checking size() == 1 && front() ==
				//   terminator.scope is equivalent to "the only ending scope is the terminator's".
				//
				// If we instead appended destructors from deeper scopes directly before the
				// terminator, the second pass would observe a scope transition between those
				// destructors and the terminator and would emit unneeded ScopeStart/ScopeEnd flags
				// around it.
				bool safe_to_opt = paths_identical
				                && (!boundary_crossed
				                    || (first_path_ending_scopes->size() == 1
				                        && first_path_ending_scopes->front() == terminator.scope));

				if (safe_to_opt) {
					// Opt: If all paths require the same destructors we don't create a new block
					// and insert them directly to the current block.
					if (boundary_crossed) {
						add_destructors_to_instr_vec(
							first_path_ending_scopes.value(), new_instructions, move_state_info
						);
					}
				} else {
					// Paths are not identical or there would be a scope regression. Create a new
					// block and insert destructors there.
					for (auto succ: successors) {
						auto& succ_ending_scopes = ending_scopes_per_succ.at(succ);

						// If no scope boundary is crossed, the edge is clean.
						if (succ_ending_scopes.empty()) continue;

						// Otherwise we have crossed a scope boundary. We have to split the edge
						// into an intermediate block.
						BlockID                  new_block_id = get_new_block_id();
						std::vector<Instruction> new_block_instructions;

						// Enter the block and preserve the caller scope to have a clear place where
						// scope changes.
						new_block_instructions.push_back(Instruction{
							Operation::Nop, {}, {}, {}, terminator.scope });

						// Add all needed destructors.
						add_destructors_to_instr_vec(
							succ_ending_scopes, new_block_instructions, move_state_info
						);

						auto new_terminator_scope = [&]() {
							// If there is only one succ_ending_scope and is equal to the
							// terminator.scope the scope of the new terminator is the same as
							// the original terminator scope (to match the optimization case).
							// Otherwise, we want to have the new terminator scope to be scope
							// of the succ_begin_scope.
							if (succ_ending_scopes.size() == 1
							    && succ_ending_scopes.front() == terminator.scope) {
								return terminator.scope;
							} else {
								return function.blocks[succ].beginScope();
							}
						}();
						Instruction new_block_terminator{
							Operation::Jump, {}, { MIRValue(succ) }, {}, new_terminator_scope
						};

						// Add the new block.
						function.blocks.put(
							new_block_id,
							Block{
								.id           = new_block_id,
								.instructions = std::move(new_block_instructions),
								.terminator   = std::move(new_block_terminator),
								.debug_name   = base::StrID("scope_end.destructors"),
							}
						);

						new_blocks_order.push_back(new_block_id);

						// Modify the caller terminator to jump through the newly created block.
						for (auto& arg: terminator.arguments) {
							variant_match(arg.getVariant()) {
								variant_case(BlockID, block) {
									if (block == succ) arg = MIRValue(new_block_id);
								}
							}
						}
					}
				}
			}
			new_blocks_instructions.push_back(std::move(new_instructions));
		}

		for (usize i = 0; i < function.block_order.size(); i++) {
			auto block_id                          = function.block_order[i];
			function.blocks[block_id].instructions = std::move(new_blocks_instructions[i]);
		}

		// Block order might have changed if new blocks where added.
		function.block_order = std::move(new_blocks_order);
	}

	void AddAssignmentDestructorsPass::run(
		query::Context& ctx, Function& function, const LifetimePassArgs& args
	) {
		for (auto block_id: function.block_order) {
			auto& block = *function.blocks.at(block_id);

			// Replay move-state through the block, starting from its entry state. Unreachable blocks
			// have no computed move-state, so default to empty (matching AddDestructorsPass).
			LocalMoveStateMap move_state_info;
			if (auto found = args.move_states.block_in_move_state.atMaybe(block_id))
				move_state_info = *found.value();

			std::vector<Instruction> new_instructions;
			new_instructions.reserve(block.instructions.size());

			for (const auto& instr: block.instructions) {
				// The destructor of the overwritten value runs before the assignment, and is
				// checked against the move-state *before* the instruction executes.
				if_opt_some(assignmentDestructorFor(ctx, instr, move_state_info), dtor_instr) {
					new_instructions.push_back(std::move(dtor_instr));
				}

				new_instructions.push_back(instr);
				move_state_info.updateMoveStateMapByInstr(instr, args.locals_by_scope);
			}

			block.instructions = std::move(new_instructions);
		}
	}
}
