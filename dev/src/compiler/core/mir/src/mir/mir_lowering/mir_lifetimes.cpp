#include "mir_lifetimes.hpp"

#include "../mir_structure/mir_structure.hpp"
#include "mir/mir_structure/mir_lifetime_scope.hpp"

#include "base/extend_cpp/variant_match.hpp"

namespace compiler::mir {
	using LocalsByScopeMap = base::HashMap<ScopeRef, std::vector<MIRLocalRef>, ScopeRefHash>;

	struct LifetimePassArgs {
		LocalsByScopeMap locals_by_scope;
	};

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
	 * @return std::vector<ScopeRef>
	 */
	std::vector<ScopeRef> getEndingScopes(ScopeRef begin, ScopeRef end) {
		std::vector<ScopeRef> result;

		auto ancestor = lca(begin, end);
		while (begin != ancestor) {
			result.push_back(begin);
			begin = begin->parent.toOpt().value();
		}

		return result;
	}

	/**
	 * @brief Get the starting scopes between two consecutive instructions.
	 *
	 * The order of scopes is the same as the the order of variables they
	 * would create (i.e. the first scope in the list is the one that creates variables that are
	 * created first).
	 */
	std::vector<ScopeRef> getStartingScopes(ScopeRef begin, ScopeRef end) {
		return getEndingScopes(end, begin) | std::views::reverse | std::ranges::to<std::vector>();
	}

	void AddDestructorsPass::run(query::Context&, Function& function, const LifetimePassArgs& args) {
		// Idea of implementation:
		// For each block we iterate over instructions and add destructors after each instruction
		// (often 0 of them), based on scopes that ends there.
		//
		// The pass is divided into two steps:
		// 1) Iterate through all instructions in the block. If moving from one instruction to the
		// 	  next crosses a scope boundary, we insert the necessary destructors between them.
		// 2) Handling terminators is more complex because they can have multiple successors with
		//	different ending scopes. The general logic is to insert a new intermediate block for
		// every path that crosses a scope boundary. This block contains only the destructors
		// specific to that path and a `Jump` to the actual destination.
		//
		// OPT: Additionally, a simple optimization to not create a lot of blocks in simple cases
		// (like if-else with no ending scopes inside) is implemented. First, we check if all
		// outgoing edges require the same set of destructors. If that's true we append the
		// destructors directly to the current block to avoid creating unnecessary blocks.
		//
		// Context is unused, but left since it might be useful in the future.
		// Preserving block order is important, because of how MIR BlockIDs works.
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


		for (auto& block_id: function.block_order) {
			// Add THIS block to our new block order.
			new_blocks_order.push_back(block_id);

			// each block is considered independently
			auto& block = function.blocks[block_id];

			std::vector<Instruction> new_instructions;
			new_instructions.reserve(block.instructions.size());

			// lambdas used just to not duplicate code:
			auto add_destructor_to_instr_vec = [&](ScopeRef                  instr_scope,
			                                       MIRLocalRef               local,
			                                       std::vector<Instruction>& instructions) {
				if (local->lifetime_flags.contains(LifetimeFlag::NoDestructor)) return;

				instructions.push_back(Instruction{
					Operation::DestructIf,
					{},
					{ local },
					{
						OperationFlag{ .flag = OperationFlag::Flag::Destruct, .local = local },
					},
					instr_scope,
				});
			};

			auto add_destructors_to_instr_vec
				= [&](const auto& ending_scopes, std::vector<Instruction>& instructions) {
					  for (const auto& scope: ending_scopes) {
						  if (not locals_by_scope.contains(scope)) continue;
						  auto& locals = locals_by_scope.at(scope);
						  // Add destructors in reverse order.
						  for (auto& local: locals | std::views::reverse)
							  add_destructor_to_instr_vec(scope, local, instructions);
					  }
				  };


			for (u64 i = 0; i < block.instructions.size(); i++) {
				const auto& instr      = block.instructions.at(i);
				const auto& next_instr = i < block.instructions.size() - 1
				                           ? block.instructions.at(i + 1)
				                           : block.terminator;

				new_instructions.push_back(instr);

				auto ending_scopes = getEndingScopes(instr.scope, next_instr.scope);

				add_destructors_to_instr_vec(ending_scopes, new_instructions);
			}

			// we handle terminator in special way,
			// because its successors are a set, not a single object:

			auto& terminator = block.terminator;
			auto  successors = getTerminatorSuccessors(terminator);
			if (successors.empty()) {
				// the function ends
				auto ending_scopes
					= getEndingScopes(terminator.scope, function.lifetime_scope_tree.root);
				add_destructors_to_instr_vec(ending_scopes, new_instructions);
			} else {
				// Here we handle situations where a branch may cause two different sets of
				// destructors being performed. For example breaking from a loop etc.
				// TODOP: Link a proper itest here.

				// For blocks with successors, we identify all unique outgoing edges.
				// This is needed to create only one block for Branch COND, B1, B1
				// TODOP: Write a test for that somehow?
				std::vector<BlockID> unique_successors;
				for (auto s: successors) {
					bool found = false;
					for (auto u: unique_successors) {
						if (u == s) {
							found = true;
							break;
						}
					}
					if (!found) unique_successors.push_back(s);
				}

				base::Optional<std::vector<ScopeRef>>         first_path_ending_scopes;
				bool                                          paths_identical  = true;
				bool                                          boundary_crossed = false;
				base::HashMap<BlockID, std::vector<ScopeRef>> ending_scopes_per_succ;

				for (auto succ: unique_successors) {
					auto succ_begin_scope  = function.blocks[succ].beginScope();
					auto succ_ending_scope = getEndingScopes(terminator.scope, succ_begin_scope);
					auto succ_starting_scopes
						= getStartingScopes(terminator.scope, succ_begin_scope);

					if (!succ_ending_scope.empty() && !succ_starting_scopes.empty())
						boundary_crossed = true;

					// Opt: Check if all path lead to the same ending scope.
					if (!first_path_ending_scopes.has_value())
						first_path_ending_scopes = succ_ending_scope;
					else if (*first_path_ending_scopes != succ_ending_scope)
						paths_identical = false;
					ending_scopes_per_succ.put(succ, std::move(succ_ending_scope));
				}

				if (paths_identical && boundary_crossed) {
					// Opt: If all paths require the same destructors we don't create a new block
					// and insert them directly to the current block.
					add_destructors_to_instr_vec(first_path_ending_scopes.value(), new_instructions);
				} else if (!paths_identical) {
					// Paths are not identical. Create a new block and insert destructors there.
					for (auto succ: unique_successors) {
						auto  succ_begin_scope  = function.blocks[succ].beginScope();
						auto& succ_ending_scope = ending_scopes_per_succ.at(succ);
						auto  succ_starting_scopes
							= getStartingScopes(terminator.scope, succ_begin_scope);

						// If no scope boundary is crossed, the edge is clean.
						if (succ_ending_scope.empty() && succ_starting_scopes.empty()) continue;

						// Otherwise we have crossed a scope boundary. We have to split the edge
						// into an intermediate block.
						BlockID                  new_block_id = get_new_block_id();
						std::vector<Instruction> new_block_instructions;

						// Enter the block and preserve the caller scope.
						new_block_instructions.push_back(Instruction{
							Operation::Nop, {}, {}, {}, terminator.scope });

						// Add all needed destructors.
						add_destructors_to_instr_vec(succ_ending_scope, new_block_instructions);

						Instruction new_block_terminator{
							Operation::Jump, {}, { MIRValue(succ) }, {}, succ_begin_scope
						};

						// Add the new block.
						function.blocks.put(
							new_block_id,
							Block{
								.id           = new_block_id,
								.instructions = std::move(new_block_instructions),
								.terminator   = std::move(new_block_terminator),
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

	template<OperationFlag::Flag scope_flag, bool reverse_local_order>
	void addScopeFlagForScopes(
		Instruction&                 instr,
		const LocalsByScopeMap&      locals_by_scope,
		const std::vector<ScopeRef>& scopes_ordered
	) {
		for (auto scope: scopes_ordered) {
			if (not locals_by_scope.contains(scope)) continue;

			auto& locals = locals_by_scope.at(scope);
			auto  proj   = [&]() -> decltype(auto) {
                if constexpr (reverse_local_order)
                    return std::views::reverse;
                else
                    return std::views::all;
			}();

			for (auto& local: locals | proj) {
				if (local->lifetime_flags.contains(LifetimeFlag::NoScopeFlags)) continue;
				instr.flags.push_back(OperationFlag{ .flag = scope_flag, .local = local });
			}
		}
	}

	constexpr auto addStartScopeFlags  // NOLINT
		= addScopeFlagForScopes<OperationFlag::Flag::ScopeStart, false>;

	constexpr auto addEndScopeFlags  // NOLINT
		= addScopeFlagForScopes<OperationFlag::Flag::ScopeEnd, true>;

	void AddScopeFlagsPass::run(query::Context&, Function& function, const LifetimePassArgs& args) {
		// Idea of implementation: for each block we iterate over instructions
		// and add destructors after each instruction (often 0 of them),
		// based on scopes that ends there.
		// context is unused, but left since it might be useful in the future.
		// preserving block order is important, because of how MIR BlockIDs works

		auto&                                         locals_by_scope = args.locals_by_scope;
		base::HashMap<BlockID, std::vector<ScopeRef>> starting_scopes_by_block;


		auto   first_block_id = function.block_order[0];
		Block& first_block    = function.blocks[first_block_id];
		addStartScopeFlags(
			first_block.firstInstruction(),
			locals_by_scope,
			getStartingScopes(
				function.lifetime_scope_tree.root, first_block.firstInstruction().scope
			)
		);

		for (auto& block_id: function.block_order) {
			// each block is considered independently
			auto& block = function.blocks[block_id];


			for (u64 i = 0; i < block.instructions.size(); i++) {
				auto& instr      = block.instructions.at(i);
				auto& next_instr = i < block.instructions.size() - 1 ? block.instructions.at(i + 1)
				                                                     : block.terminator;

				// We add the start scope flags at the beginning of the second instruction.
				auto starting_scopes = getStartingScopes(instr.scope, next_instr.scope);
				addStartScopeFlags(next_instr, locals_by_scope, starting_scopes);

				// We add the end scope flags at the end of the first instruction we compare
				auto ending_scopes = getEndingScopes(instr.scope, next_instr.scope);
				addEndScopeFlags(instr, locals_by_scope, ending_scopes);
			}

			auto& terminator = block.terminator;
			auto  successors = getTerminatorSuccessors(terminator);
			if (successors.empty()) {
				// End scope flags for function end
				auto ending_scopes
					= getEndingScopes(terminator.scope, function.lifetime_scope_tree.root);
				addEndScopeFlags(terminator, locals_by_scope, ending_scopes);
				continue;
			}

			// Successors not empty
			base::Optional<std::vector<ScopeRef>> ending_scopes;

			// We add the end scope flags at the end of the terminator,
			// and start scope flags at the beginning of the first instruction of the successor blocks.
			for (auto succ: successors) {
				auto& succ_block = function.blocks[succ];
				auto  succ_ending_scopes
					= getEndingScopes(terminator.scope, succ_block.beginScope());

				if (ending_scopes.has_value()) {
					// we have to validate that all paths have the same ending scopes
					// otherwise this implementation is incorrect
					CORE_ASSERT(ending_scopes == succ_ending_scopes, "Different ending scopes");
				} else {
					ending_scopes.emplace(std::move(succ_ending_scopes));
					addEndScopeFlags(terminator, locals_by_scope, ending_scopes.value());
				}

				auto starting_scopes
					= getStartingScopes(terminator.scope, succ_block.firstInstruction().scope);

				if (auto existing_starting_scopes = starting_scopes_by_block.atMaybe(succ)) {
					CORE_ASSERT(
						*existing_starting_scopes.value() == starting_scopes,
						"Different starting scopes for the same block"
					);
				} else {
					addStartScopeFlags(
						succ_block.firstInstruction(), locals_by_scope, starting_scopes
					);
					starting_scopes_by_block.put(succ, std::move(starting_scopes));
				}
			}
		}
	}

	/**
	 * @brief Constructs LifetimePassArgs for given function.
	 */
	LifetimePassArgs constructLifetimePassArgs(const Function& function) {
		LifetimePassArgs args;

		for (const auto& local: function.local_list)
			if (local.scope.value() != function.no_lifetime_scope)
				args.locals_by_scope.put(local.scope.value()).first->second.emplace_back(&local);

		return args;
	}

	Function runAllLifetimePasses(query::Context& ctx, Function function) {
		auto args = constructLifetimePassArgs(function);

		// The order here does matter. AddDestructorPass{} performs a transformation on the CFG
		// which adds an important invariant that all successors of a block have the same ending
		// scopes. This assumption is then used when adding ScopeFlags.
		AddDestructorsPass{}.run(ctx, function, args);
		AddScopeFlagsPass{}.run(ctx, function, args);

		return function;
	}
}
