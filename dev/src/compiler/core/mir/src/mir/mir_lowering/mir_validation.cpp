#include "../mir_structure/mir_structure.hpp"
#include "errors.hpp"
#include "mir_lifetimes.hpp"

#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/symbols/symbol_id_utils.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>

#include <algorithm>

namespace compiler::mir {

	class Worklist final {
		std::vector<BlockID> stack;
		std::vector<bool>    on_stack;

	public:
		Worklist(usize block_count): stack{}, on_stack(block_count) {}

		void push(BlockID id) {
			if (not on_stack.at(id.asInt())) {
				stack.push_back(id);
				on_stack.at(id.asInt()) = true;
			}
		}

		BlockID pop() {
			auto top                 = stack.back();
			on_stack.at(top.asInt()) = false;
			return top;
		}

		BlockID top() { return stack.back(); }
	};

	enum class Status { Alive, Moved, MaybeMoved };


	using LocalStatusMap = base::HashMap<LocalID, Status>;

	// Flat lattice join: equal statuses (including both-uninitialized) are preserved, any
	// disagreement collapses to `MaybeMoved` (the lattice top).
	base::Optional<Status> joinStatus(base::Optional<Status> a, base::Optional<Status> b) {
		if (a.has_value() && b.has_value())
			if (a == b)
				return a;
			else
				return Status::MaybeMoved;
		else
			return {};
	}

	LocalStatusMap joinMaps(const LocalStatusMap& a, const LocalStatusMap& b) {
		LocalStatusMap result;
		for (const auto& [local, status]: a)
			if (auto joined = joinStatus(status, b.atMaybeCopy(local)))
				result.insertOrAssign(local, *joined);

		for (const auto& [local, status]: b) {
			if (a.contains(local)) continue;  // already handled above

			if (auto joined = joinStatus({}, status)) result.insertOrAssign(local, *joined);
		}
		return result;
	}

	bool sameMap(const LocalStatusMap& a, const LocalStatusMap& b) {
		if (a.size() != b.size()) return false;
		for (const auto& [local, status]: a) {
			auto other = b.atMaybe(local);
			if (not other || *other.value() != status) return false;
		}
		return true;
	}

	LocalStatusMap transferBlock(const Block& block, LocalStatusMap map) {
		auto apply_flags = [&map](const std::vector<OperationFlag>& flags) {
			for (const auto& flag: flags) {
				switch (flag.flag) {
				case OperationFlag::Flag::Construct:
					map.insertOrAssign(flag.local->id, Status::Alive);
					break;
				case OperationFlag::Flag::Move:
					map.insertOrAssign(flag.local->id, Status::Moved);
					break;
				default:
					break;
				}
			}
		};
		for (const auto& instr: block.instructions) apply_flags(instr.flags);
		apply_flags(block.terminator.flags);
		return map;
	}

	// Collects locals that are *read* by a value/place. Index projections read the locals used as
	// indices; a place's base is read unless `include_base` is false (used for assignment targets
	// without projections, which only define the local).
	void collectValueReads(const MIRValue& value, std::vector<MIRLocalRef>& out);

	void collectPlaceReads(const MIRPlace& place, bool include_base, std::vector<MIRLocalRef>& out) {
		if (include_base && place.isLocal()) out.push_back(place.getBase<MIRLocalRef>());
		for (const auto& proj: place.projection_chain) {
			variant_match(proj.storage) {
				variant_case(MIRPlace::IndexProjection, index) {
					collectValueReads(*index.index, out);
				}
				variant_default {}
			}
		}
	}

	void collectValueReads(const MIRValue& value, std::vector<MIRLocalRef>& out) {
		variant_match(value.getVariant()) {
			variant_case(MIRPlace, place) { collectPlaceReads(place, true, out); }
			variant_default {}
		}
	}

	std::vector<MIRLocalRef> instructionReads(const Instruction& instr) {
		std::vector<MIRLocalRef> reads;
		for (const auto& arg: instr.arguments) collectValueReads(arg, reads);
		// An assignment to a whole local (no projections) defines it; only projected writes read
		// the base (e.g. `a.b = x` reads `a`).
		if (instr.output && instr.output->hasProjections())
			collectPlaceReads(*instr.output, true, reads);
		return reads;
	}

	// Forward dataflow move/initialization analysis. Computes the status of every tracked local at
	// each program point and reports use-before-initialization and use-after-move.
	base::OkBad validateMoves(query::Context& ctx, const Function& fun) {
		if (fun.block_order.empty()) return base::OK;

		auto is_tracked = [&](MIRLocalRef local) {
			return local->scope.has_value() && local->scope.value() != fun.no_lifetime_scope
			    && not local->lifetime_flags.contains(LifetimeFlag::NoUseAfterFreeValidation);
		};

		// Parameters are alive on function entry.
		LocalStatusMap seed;
		for (const auto& local: fun.local_list) {
			MIRLocalRef ref = base::Ref(&local);
			if (ref->parameter_index.has_value() && is_tracked(ref))
				seed.insertOrAssign(ref->id, Status::Alive);
		}

		// Predecessor lists.
		base::HashMap<BlockID, std::vector<BlockID>> predecessors;
		for (auto block_id: fun.block_order)
			for (auto succ: getTerminatorSuccessors(fun.blocks.at(block_id)->terminator))
				predecessors.put(succ).first->second.push_back(block_id);

		base::HashMap<BlockID, LocalStatusMap> out_status;  // keys == reachable blocks

		auto entry      = fun.block_order.front();
		auto compute_in = [&](BlockID block_id) {
			base::Optional<LocalStatusMap> acc;
			if (block_id == entry) acc = seed;
			if (auto preds = predecessors.atMaybe(block_id))
				for (auto pred: *preds.value()) {
					auto pred_out = out_status.atMaybe(pred);
					if (not pred_out) continue;  // not yet reachable/processed
					acc = acc ? joinMaps(*acc, *pred_out.value()) : *pred_out.value();
				}
			return acc ? std::move(*acc) : LocalStatusMap{};
		};

		// Worklist fixpoint, seeded from the entry so only reachable blocks are processed.
		std::vector<BlockID>         stack{ entry };
		base::HashMap<BlockID, bool> on_stack;
		on_stack.put(entry, true);
		while (not stack.empty()) {
			auto block_id = stack.back();
			stack.pop_back();
			on_stack.erase(block_id);

			auto new_out = transferBlock(*fun.blocks.at(block_id), compute_in(block_id));

			auto existing = out_status.atMaybe(block_id);
			if (existing && sameMap(*existing.value(), new_out)) continue;
			out_status.insertOrAssign(block_id, std::move(new_out));

			for (auto succ: getTerminatorSuccessors(fun.blocks.at(block_id)->terminator))
				if (not on_stack.contains(succ)) {
					on_stack.put(succ, true);
					stack.push_back(succ);
				}
		}

		// Checking pass: replay each reachable block from its converged input map and validate
		// every use. Each local is reported at most once to avoid cascades.
		bool                         had_error = false;
		base::HashMap<LocalID, bool> reported;
		auto check_use = [&](MIRLocalRef local, const LocalStatusMap& status_map) {
			if (not is_tracked(local)) return;
			auto status = effectiveStatus(status_map, local->id);
			if (status.has_value() && *status == Status::Alive) return;
			if (reported.contains(local->id)) return;
			reported.put(local->id, true);
			had_error = true;

			const char* title = not status.has_value() ? "Use of uninitialized variable"
			                  : *status == Status::Moved
			                      ? "Use after move"
			                      : "Use of a possibly moved or uninitialized variable";
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				title,
				base::strConcat(
					"The variable `",
					local->getName(),
					"` is used here but it is not ",
					"guaranteed to hold a valid value at this point."
				)
			));
		};

		auto check_instruction = [&](const Instruction& instr, LocalStatusMap& status_map) {
			for (auto local: instructionReads(instr)) check_use(local, status_map);
			for (const auto& flag: instr.flags)
				if (flag.flag == OperationFlag::Flag::Move) check_use(flag.local, status_map);
			applyFlags(instr.flags, status_map);
		};

		for (auto block_id: fun.block_order) {
			if (not out_status.contains(block_id)) continue;  // unreachable
			const Block&   block      = *fun.blocks.at(block_id);
			LocalStatusMap status_map = compute_in(block_id);
			for (const auto& instr: block.instructions) check_instruction(instr, status_map);
			check_instruction(block.terminator, status_map);
		}

		return had_error ? base::BAD : base::OK;
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
								return helios::maybeSymbolPst(local_ref->helios_id.value())
								    .map([&](const auto& pst) {
										return pst.unlock(ctx)->getStablePosition();
									});
							};
							auto shadowing_pos = get_pos(shadowing);
							auto shadowed_pos  = get_pos(shadowed);

							if (shadowing_pos && shadowed_pos) {
								auto msg = makeBox<VariableShadowingError>(*shadowing_pos);
								msg->addAttachedMessage(
									makeBox<ShadowedDeclarationNote>(*shadowed_pos)
								);
								ctx.logInt(std::move(msg));
							} else {
								ctx.logInt(makeBox<dia_int::PlaceholderError>(
									"Variable declaration shadows a previous declaration.",
									base::strConcat(
										"The exact code location is unavailable because the "
										"variable is compiler generated. ",
										"The shadowing happened for the symbol `",
										shadowing->getName(),
										"`."
									)
								));
							}
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

	base::OkBad validateFunction(query::Context& ctx, const Function& fun) {
		bool all_ok = validateMoves(ctx, fun).isOk() && validateShadowing(ctx, fun).isOk();
		return all_ok ? base::OK : base::BAD;
	}
}
