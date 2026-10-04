#include "mir_lifetimes.hpp"

#include "../mir_structure/mir_structure.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/queries/types.hpp>
#include <mir/mir_lowering/mir_liveness.hpp>

#include <algorithm>
#include <set>

namespace compiler::mir {

	namespace {
		/**
		 * @brief The new state of every lifetime flag local that @p instr changes. `true` means
		 * the local owns a value from here on, `false` that it was moved out of.
		 */
		std::vector<std::pair<MIRLocalRef, bool>> lifetimeFlagWrites(
			const Instruction& instr, const base::Map<MIRLocalRef, MIRLocalRef>& flag_of
		) {
			std::vector<std::pair<MIRLocalRef, bool>> writes;

			for (const auto& flag: instr.flags) {
				bool alive = false;
				switch (flag.flag) {
					using enum OperationFlag::Flag;
				case Construct:
				case Reinit:
					alive = true;
					break;
				case Move:
					alive = false;
					break;
				default:
					continue;
				}

				auto flag_local = flag_of.atMaybe(flag.local);
				if (flag_local.empty()) continue;

				auto previous = std::ranges::find_if(writes, [&](const auto& write) {
					return write.first == *flag_local.value();
				});
				if (previous == writes.end())
					writes.emplace_back(*flag_local.value(), alive);
				else if (alive)
					previous->second = true;
			}

			return writes;
		}
	}

	void AddLifetimeFlagsLocalsPass::run(query::Context&, Function& function, const LifetimePassArgs&) {
		// Get the local that is destructed by a DestructIf.
		auto conditionally_destructed_local
			= [](const Instruction& instr) -> base::Optional<MIRLocalRef> {
			if (instr.operation != Operation::DestructIf) return {};
			if (instr.arguments.size() < 2) return {};

			const auto& place = instr.arguments.at(1).get<MIRPlace>();
			CORE_ASSERT(place.isLocal(), "A conditional destruction of a non-local place");
			return place.getBase<MIRLocalRef>();
		};

		// Only locals that are destructed conditionally need a flag.
		std::set<MIRLocalRef> conditionally_destructed;
		for (auto block_id: function.block_order)
			for (const auto& instr: function.blocks.at(block_id)->instructions)
				if_opt_some(conditionally_destructed_local(instr), local)
					conditionally_destructed.insert(local);

		if (conditionally_destructed.empty()) return;

		// Collect all locals tracked by this pass.
		std::vector<MIRLocalRef> tracked_locals;
		for (const auto& local: function.local_list)
			if (conditionally_destructed.contains(&local)) tracked_locals.emplace_back(&local);

		const auto bool_type = tsh::SymbolType<>::withDefaults(tsh::getBoolType());

		// Add lifetime flags for all of them.
		base::Map<MIRLocalRef, MIRLocalRef> flag_of;
		for (auto local: tracked_locals)
			flag_of.put(
				local,
				function.addGeneratedLocal(
					bool_type,
					local->scope.value(),
					LifetimeFlag::NoDestructor | LifetimeFlag::NoMoveStatusValidation
				)
			);

		auto flag_write = [&](MIRLocalRef flag, bool alive, const Instruction& at) {
			return Instruction{
				Operation::Assign,
				MIRPlace(flag),
				{ MIRConstant{ ctv::CompileTimeValue(alive) } },
				{},
				at.scope,
				NoInstrParameters{},
				at.metadata,
			};
		};

		// Flags of the parameters, in the order their scope is opened in the entry block.
		std::vector<MIRLocalRef> parameter_flags;

		for (auto block_id: function.block_order) {
			auto& block = *function.blocks.at(block_id);

			std::vector<Instruction> new_instructions;
			new_instructions.reserve(block.instructions.size());

			// In the first block, set all parameter lifetime flags to true.
			if (block_id == function.block_order.front()) {
				for (auto local: tracked_locals)
					if (local->parameter_index.has_value()) {
						auto flag = flag_of.at(local);
						new_instructions.push_back(flag_write(flag, true, block.firstInstruction()));
						new_instructions.back().flags.push_back(OperationFlag{
							.flag = OperationFlag::Flag::ScopeStart, .local = flag });
						parameter_flags.push_back(flag);
					}
			}

			auto append_flag_writes = [&](const Instruction& instr) {
				for (auto [flag, alive]: lifetimeFlagWrites(instr, flag_of))
					new_instructions.push_back(flag_write(flag, alive, instr));
			};

			// The flag locals are created after `AddScopeFlagsPass`, so they have no scope flags of
			// their own yet. DVM pairs `init`/`deinit` on a stack, so a flag has to be opened and
			// closed right next to the local it guards: its `ScopeStart` goes directly after the
			// `ScopeStart` of that local, and its `ScopeEnd` directly before the `ScopeEnd` of it.
			auto adjust_scopes = [&](Instruction& instr) {
				std::vector<OperationFlag> scope_starts;
				std::vector<OperationFlag> other_flags;
				std::vector<Instruction>   flag_inits;
				other_flags.reserve(instr.flags.size());

				for (const auto& flag: instr.flags) {
					auto flag_local = flag_of.atMaybe(flag.local);

					switch (flag.flag) {
						using enum OperationFlag::Flag;
					case ScopeStart:
						scope_starts.push_back(flag);
						if (flag_local.has_value()) {
							scope_starts.push_back(OperationFlag{ .flag  = ScopeStart,
							                                      .local = *flag_local.value() });
							flag_inits.push_back(flag_write(*flag_local.value(), false, instr));
						}
						break;
					case ScopeEnd:
						if (flag_local.has_value())
							other_flags.push_back(OperationFlag{ .flag  = ScopeEnd,
							                                     .local = *flag_local.value() });
						other_flags.push_back(flag);
						break;
					default:
						other_flags.push_back(flag);
						break;
					}
				}

				instr.flags = std::move(other_flags);

				if (flag_inits.empty()) {
					for (auto& flag: scope_starts) instr.flags.push_back(flag);
					return;
				}

				// The first generated write takes over every scope that started on this
				// instruction, so all of the locals are alive by the time any of the writes runs.
				flag_inits.front().flags = std::move(scope_starts);
				for (auto& init: flag_inits) new_instructions.push_back(std::move(init));
			};

			for (auto& instr: block.instructions) {
				adjust_scopes(instr);
				new_instructions.push_back(instr);

				// The flag the LIR lowering branches on becomes the last argument of the drop.
				if_opt_some(conditionally_destructed_local(instr), local) new_instructions.back()
					.arguments.emplace_back(MIRPlace(flag_of.at(local)));

				append_flag_writes(instr);
			}

			adjust_scopes(block.terminator);

			append_flag_writes(block.terminator);

			block.instructions = std::move(new_instructions);
		}

		// The parameter flags are opened before anything else, so they are closed on every path
		// leaving the function, after every other scope of that path has ended.
		for (auto block_id: function.block_order) {
			auto& terminator = function.blocks.at(block_id)->terminator;
			if (not getTerminatorSuccessors(terminator).empty()) continue;

			for (auto flag: parameter_flags | std::views::reverse)
				terminator.flags.push_back(OperationFlag{ .flag  = OperationFlag::Flag::ScopeEnd,
				                                          .local = flag });
		}
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
						base::strConcat(
							"Different starting scopes for the same block ", block_id.asInt()
						)
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

	LocalsByScopeMap collectLocalsByScope(const Function& function) {
		LocalsByScopeMap locals_by_scope;
		for (const auto& local: function.local_list)
			if (local.scope.value() != function.no_lifetime_scope)
				locals_by_scope.put(local.scope.value()).first->second.emplace_back(&local);
		return locals_by_scope;
	}

	/**
	 * @brief Constructs LifetimePassArgs for given function.
	 */
	LifetimePassArgs constructLifetimePassArgs(const Function& function) {
		LifetimePassArgs args;

		args.locals_by_scope = collectLocalsByScope(function);

		// Predecessor lists.
		for (auto block_id: function.block_order) args.block_predecessors.put(block_id);

		for (auto block_id: function.block_order)
			for (auto succ: getTerminatorSuccessors(function.blocks.at(block_id)->terminator))
				args.block_predecessors.put(succ).first->second.push_back(block_id);

		args.move_states = MoveStateData::calculateGlobalInMoveStateMap(
			function, args.block_predecessors, args.locals_by_scope
		);

		return args;
	}

	Function runAllLifetimePasses(query::Context& ctx, Function function) {
		auto args = constructLifetimePassArgs(function);
		// The order here does matter. AddDestructorsPass{} performs a transformation on the CFG
		// which adds an important invariant that all successors of a block have the same ending
		// scopes. This assumption is then used when adding ScopeFlags.
		InvalidUseCheck{}.run(ctx, function, args);
		AddAssignmentDestructorsPass{}.run(ctx, function, args);
		AddDestructorsPass{}.run(ctx, function, args);
		AddScopeFlagsPass{}.run(ctx, function, args);
		AddLifetimeFlagsLocalsPass{}.run(ctx, function, args);

		return function;
	}
}
