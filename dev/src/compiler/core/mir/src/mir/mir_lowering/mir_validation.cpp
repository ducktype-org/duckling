#include "../mir_structure/mir_structure.hpp"
#include "errors.hpp"
#include "mir_lifetimes.hpp"
#include "mir_queries.hpp"

#include <bits/stdc++.h>
#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/symbols/symbol_id_utils.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>

#include <algorithm>
#include <unordered_set>

namespace compiler::mir {
	base::OkBad validateMoves(query::Context&, const Function& fun) {
		using LocalSet      = std::unordered_set<LocalID>;
		using BlockLocalSet = base::HashMap<BlockID, LocalSet>;

		BlockLocalSet
			moved_variables;  // Variables moved in Block, they can't be used after this Block,
		BlockLocalSet used_variables;  // variables which must be valid, at the begining of Block.

		for (const auto& block: fun.blocks) {  // Fill with blocks.
			moved_variables.emplace(block.key, LocalSet());
			used_variables.emplace(block.key, LocalSet());
		}
		base::HashMap<LocalID, BlockID>
			construction_block;  // For each Local store where it is constructed.

		// Analyze each block independently.
		for (const auto& block: fun.blocks) {
			auto process_instruction = [&](Instruction instr) {
				if (instr.operation == Operation::Destruct
				    || instr.operation == Operation::DestructIf)

					return base::OK;  // LIR decides whether destruction should be performed.

				// Firstly list all arguments - They must be valid.
				for (const auto& arg: instr.arguments) {
					if (arg.isLocal()) {
						used_variables.at(block.key).insert(
							arg.get<MIRPlace>().getBase<MIRLocalRef>()->id
						);

						if (moved_variables.at(block.key).contains(
								arg.get<MIRPlace>().getBase<MIRLocalRef>()->id
							))
							return base::BAD;  // It is already moved.
					}
				}

				// Output can't be local, already moved, variable.
				if (instr.output.has_value() && instr.output.value().isLocal()) {
					used_variables.at(block.key).insert(
						instr.output.value().getBase<MIRLocalRef>()->id
					);

					if (moved_variables.at(block.key).contains(
							instr.output.value().getBase<MIRLocalRef>()->id
						))
						return base::BAD;  // It is already moved.
				}

				for (const auto& flag: instr.flags) {
					if (flag.flag == OperationFlag::Flag::Move) {
						// @note Now we assume that variable moved in instruction, must be
						// its argument and appear exactly one time there. It can't be
						// output of instruction. It may change in the future.

						if (moved_variables[block.key].contains(flag.local->id))
							return base::BAD;  // Already moved.

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
							return base::BAD;  // Used 0 or 2 or more times as argument.

						if (instr.output.has_value() && instr.output.value().isLocal()
						    && instr.output.value().getBase<MIRLocalRef>()->id == flag.local->id)
							return base::BAD;  // Moved local used as output.
					}
					if (flag.flag == OperationFlag::Flag::Construct) {
						// Assume constructors are valid (every use is after construct).
						construction_block.emplace(flag.local->id, block.key);
					}
					// Omit destruct flag - LIR will handle it.
				}
				return base::OK;
			};

			for (const auto& instruction: block.value.instructions)
				if (process_instruction(instruction).isBad()) return base::BAD;

			if (process_instruction(block.value.terminator).isBad()) return base::BAD;
		}

		// Now we perform global analysis.

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
		for (const auto& id: fun.block_order) visited.emplace(id, States());

		for (const auto& local: construction_block) {
			for (const auto& id: fun.block_order)
				visited[id].state[USABLE] = false, visited[id].state[NOT_USABLE] = false;

			const auto& starting_block = local.second;

			auto visit = [&](this const auto& self, const BlockID& id, const int& cr_state
			             ) -> base::OkBad {
				visited[id].state[cr_state] = true;
				int next_state              = NOT_USABLE;
				if (cr_state == NOT_USABLE) {
					if (moved_variables[id].contains(local.first)
					    || used_variables[id].contains(local.first))
						return base::BAD;
					next_state = NOT_USABLE;
				} else {
					if (moved_variables[id].contains(local.first))
						next_state = NOT_USABLE;
					else
						next_state = USABLE;
				}

				for (const auto& next_block: getTerminatorSuccessors(fun.blocks[id].terminator)) {
					if (next_block != starting_block && !visited[next_block].state[next_state]) {
						if (self(next_block, next_state).isBad()) return base::BAD;
					}
				}
				return base::OK;
			};

			if (visit(starting_block, USABLE).isBad()) return base::BAD;
		}

		return base::OK;
	}

	base::OkBad validateShadowing(query::Context& ctx, const Function& fun) {
		base::HashMap<base::StrID, std::vector<CRef<MIRLocal>>> named_locals;
		for (auto& local: fun.local_list) {
			if (local.helios_id.empty()) continue;

			auto name = helios::name(*local.helios_id);
			match_optional(named_locals.atMaybe(name)) {
				opt_some(prev_defs) {
					for (auto def: *prev_defs) {
						auto lc_scope = lca(*def->scope, *local.scope);
						if (lc_scope == *def->scope || lc_scope == *local.scope) {
							// Since the LCA is one of the scopes, the other has to be contained in it.
							auto [shadowing, shadowed] = (lc_scope == *def->scope)
							                               ? std::tuple{ base::Ref(&local), def }
							                               : std::tuple{ def, base::Ref(&local) };

							auto get_pos = [&](auto local_ref) {
								return helios::symbolPst(local_ref->helios_id.value())
								    .value()
								    .unlock(ctx)
								    ->getStablePosition();
							};
							auto msg = makeBox<VariableShadowingError>(get_pos(shadowing));
							msg->addAttachedMessage(
								makeBox<ShadowedDeclarationNote>(get_pos(shadowed))
							);
							ctx.logInt(std::move(msg));
							return base::BAD;
						}
					}
					prev_defs->emplace_back(&local);
				}
				opt_none { named_locals.put(name, { &local }); }
			}
		}
		return base::OK;
	}

	base::OkBad validateNoComptimeTypes(query::Context& ctx, const Function& fun) {
		variant_match(fun.helios_id) {
			variant_case(FunctionSymID, f_id) {
				auto status_q = ctx.query<IsComptimeOnly>(f_id.id);
				if (!status_q.get()->hasFailed()
				    && status_q.get()->valueOrPanic() == ComptimeStatus::ComptimeOnly) {
					if (helios::name(f_id.id) == base::StrID("main")) {
						ctx.logInt(makeBox<dia_int::PlaceholderError>(
							"The 'main' function cannot be marked as compile-time only "
							"(comptime-only).",
							""
						));
						return base::BAD;
					}
				}
			}

			variant_case(GlobalVariableCTOR, g_id) {
				bool is_global_comptime = false;

				if (isComptimeOnlyType(fun.return_type.getType())) is_global_comptime = true;

				if (!is_global_comptime) {
					for (const auto& local: fun.local_list) {
						if (isComptimeOnlyType(local.type.getType())) {
							is_global_comptime = true;
							break;
						}
					}
				}

				if (!is_global_comptime) {
					for (auto block_id: fun.block_order) {
						if (!fun.blocks.contains(block_id)) continue;
						const auto& block = fun.blocks.at(block_id);
						for (const auto& instr: block->instructions) {
							if (isMetaOp(instr.operation)) {
								is_global_comptime = true;
								break;
							}
						}
						if (is_global_comptime) break;
					}
				}

				if (is_global_comptime) {
					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						"Global variable initializers/constructors cannot contain compile-time "
						"only expressions or types.",
						""
					));
					return base::BAD;
				}
			}

			variant_default {}
		}

		return base::OK;
	}

	base::OkBad validateFunction(query::Context& ctx, const Function& fun) {
		bool all_ok = validateMoves(ctx, fun).isOk() && validateShadowing(ctx, fun).isOk()
		           && validateNoComptimeTypes(ctx, fun).isOk();
		return all_ok ? base::OK : base::BAD;
	}
}
