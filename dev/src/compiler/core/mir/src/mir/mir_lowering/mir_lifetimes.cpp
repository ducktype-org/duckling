#include "mir_lifetimes.hpp"

#include "../mir_structure/mir_structure.hpp"

namespace compiler::mir {
	struct LifetimePassArgs {
		std::map<ScopeRef, std::vector<MIRLocalRef>> locals_by_scope;
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
		// Idea of implementation: for each block we iterate over instructions
		// and add destructors after each instruction (often 0 of them),
		// based on scopes that ends there.
		// context is unused, but left since it might be useful in the future.
		// preserving block order is important, because of how MIR BlockIDs works

		// No lifetime analysis here, since it is quite complex.
		// See doc-comment of this function for details.

		auto&                                 locals_by_scope = args.locals_by_scope;
		std::vector<std::vector<Instruction>> new_blocks_instructions;

		for (auto& block_id: function.block_order) {
			// each block is considered independently
			auto& block = function.blocks[block_id];

			std::vector<Instruction> new_instructions;
			new_instructions.reserve(block.instructions.size());

			// lambdas used just to not duplicate code:
			auto add_destructor = [&](ScopeRef instr_scope, MIRLocalRef local) {
				if (local->lifetime_flags.contains(LifetimeFlag::NoDestructor)) return;

				new_instructions.push_back(Instruction{
					Operation::DestructIf,
					{},
					{ local },
					{
						OperationFlag{ .flag = OperationFlag::Flag::Destruct, .local = local },
					},
					instr_scope,
				});
			};
			auto add_destructors = [&](const auto& ending_scopes) {
				for (auto scope: ending_scopes) {
					if (not locals_by_scope.contains(scope)) continue;
					auto& locals = locals_by_scope.at(scope);
					for (auto& local: locals | std::views::reverse) add_destructor(scope, local);
				}
			};

			for (u64 i = 0; i < block.instructions.size(); i++) {
				const auto& instr      = block.instructions.at(i);
				const auto& next_instr = i < block.instructions.size() - 1
				                           ? block.instructions.at(i + 1)
				                           : block.terminator;

				new_instructions.push_back(instr);

				auto ending_scopes = getEndingScopes(instr.scope, next_instr.scope);

				add_destructors(ending_scopes);
			}

			// we handle terminator in special way,
			// because its successors are a set, not a single object:

			auto& terminator = block.terminator;
			auto  successors = getTerminatorSuccessors(terminator);
			if (successors.empty()) {
				// the function ends
				auto ending_scopes
					= getEndingScopes(terminator.scope, function.lifetime_scope_tree.root);
				add_destructors(ending_scopes);
			} else {
				base::Optional<std::vector<ScopeRef>> ending_scopes;

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

				add_destructors(ending_scopes.value());
			}

			new_blocks_instructions.push_back(std::move(new_instructions));
		}

		for (usize i = 0; i < function.block_order.size(); i++) {
			auto block_id                          = function.block_order[i];
			function.blocks[block_id].instructions = std::move(new_blocks_instructions[i]);
		}
	}

	template<OperationFlag::Flag scope_flag, bool reverse_local_order>
	void addScopeFlagForScopes(
		Instruction&                                        instr,
		const std::map<ScopeRef, std::vector<MIRLocalRef>>& locals_by_scope,
		const std::vector<ScopeRef>&                        starting_scopes
	) {
		for (auto scope: starting_scopes) {
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


		// No lifetime analysis here, since it is quite complex.
		// See doc-comment of this function for details.

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
				base::Optional<Instruction> previous_instr
					= i > 0 ? (block.instructions.at(i - 1)) : base::Optional<Instruction>{};

				auto&       instr      = block.instructions.at(i);
				const auto& next_instr = i < block.instructions.size() - 1
				                           ? block.instructions.at(i + 1)
				                           : block.terminator;


				if (previous_instr.has_value()) {
					// We add the start scope flags at the beginning of the second instruction.
					auto starting_scopes = getStartingScopes(previous_instr->scope, instr.scope);
					addStartScopeFlags(instr, locals_by_scope, starting_scopes);
				}

				// We add the end scope flags at the end of the first instruction we compare
				auto ending_scopes = getEndingScopes(instr.scope, next_instr.scope);
				addEndScopeFlags(instr, locals_by_scope, ending_scopes);
			}

			//  Start scope flags between the last instruction and the terminator:
			if (not block.instructions.empty()) {
				auto& last_instr      = block.instructions.back();
				auto  starting_scopes = getStartingScopes(last_instr.scope, block.terminator.scope);
				addStartScopeFlags(block.terminator, locals_by_scope, starting_scopes);
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
				args.locals_by_scope[local.scope.value()].emplace_back(&local);

		return args;
	}

	Function runAllLifetimePasses(query::Context& ctx, Function function) {
		auto args = constructLifetimePassArgs(function);

		// The order here probably does not matter, but maybe for more safety
		// we should have AddScopeFlagsPass{} run after AddDestructorsPass{},
		// so that no destructors are run after scope end flags.
		AddDestructorsPass{}.run(ctx, function, args);
		AddScopeFlagsPass{}.run(ctx, function, args);

		return function;
	}
}
