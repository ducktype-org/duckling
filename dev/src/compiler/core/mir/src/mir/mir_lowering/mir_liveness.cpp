#include "mir_liveness.hpp"
#include "mir_lifetimes.hpp"

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

		bool empty() { return stack.empty(); }

		BlockID top() { return stack.back(); }
	};

	base::Optional<Status> joinStatus(base::Optional<Status> a, base::Optional<Status> b) {
		if (a.has_value() && b.has_value())
			if (a == b)
				return a;
			else
				return Status::MaybeMoved;
		else
			return {};
	}

	/**
	 * @brief If the value is not present in one of the maps, then we assume that is it
	 * uninitialized in the merge, as we don't have the destructors inserted there yet.
	 */
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

	bool sameMap(base::Optional<CRef<LocalStatusMap>> a_opt, const LocalStatusMap& b) {
		if (a_opt.empty()) return false;
		auto a = a_opt.value();
		if (a->size() != b.size()) return false;
		for (const auto& [local, status]: *a) {
			auto other = b.atMaybe(local);
			if (not other || *other.value() != status) return false;
		}
		return true;
	}

	void updateLivenessMapByInstr(LocalStatusMap& map, const Instruction& instr) {
		for (const auto& flag: instr.flags) {
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
	}

	LocalStatusMap transferBlock(const Block& block, LocalStatusMap map) {
		for (const auto& instr: block.instructions) updateLivenessMapByInstr(map, instr);
		updateLivenessMapByInstr(map, block.terminator);
		return map;
	}

	LivenessData calculateGlobalInLivenessStatus(
		const Function& fun, const base::HashMap<BlockID, std::vector<BlockID>>& block_predecessors
	) {
		if (fun.block_order.empty()) return {};

		auto is_tracked = [&](MIRLocalRef local) {
			return local->scope.has_value() && local->scope.value() != fun.no_lifetime_scope
			    && not local->lifetime_flags.contains(LifetimeFlag::NoUseAfterFreeValidation);
		};

		// Parameters are alive on function entry.
		LocalStatusMap in_map_from_params;
		for (const auto& local: fun.local_list) {
			MIRLocalRef ref = base::Ref(&local);
			if (ref->parameter_index.has_value() && is_tracked(ref))
				in_map_from_params.insertOrAssign(ref->id, Status::Alive);
		}


		base::HashMap<BlockID, LocalStatusMap> out_status;
		base::HashMap<BlockID, LocalStatusMap> in_status;

		auto entry = fun.block_order.front();

		auto compute_in = [&](BlockID block_id) {
			base::Optional<LocalStatusMap> acc;
			if (block_id == entry) acc = in_map_from_params;
			if (auto preds = block_predecessors.atMaybe(block_id))
				for (auto pred: *preds.value()) {
					auto pred_out = out_status.atMaybe(pred);
					if (not pred_out) continue;  // not yet reachable/processed
					acc = acc ? joinMaps(*acc, *pred_out.value()) : *pred_out.value();
				}
			return acc ? std::move(*acc) : LocalStatusMap{};
		};

		// Worklist fixpoint, seeded from the entry so only reachable blocks are processed.
		Worklist worklist(fun.block_order.size());
		worklist.push(entry);
		while (not worklist.empty()) {
			auto block_id = worklist.pop();
			auto new_in   = compute_in(block_id);
			auto new_out  = transferBlock(*fun.blocks.at(block_id), new_in);

			if (sameMap(std::as_const(out_status).atMaybe(block_id), new_out)) continue;
			out_status.insertOrAssign(block_id, std::move(new_out));
			in_status.insertOrAssign(block_id, std::move(new_in));

			for (auto succ: getTerminatorSuccessors(fun.blocks.at(block_id)->terminator))
				worklist.push(succ);
		}
		return { .block_in_liveness = in_status };
	}

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

	void InvalidUseCheck::run(query::Context& ctx, Function& fun, const LifetimePassArgs& args) {
		if (fun.block_order.empty()) return;

		// Same tracking predicate as the liveness fixpoint: only locals that participate in
		// lifetime analysis are validated. Untracked temporaries / `no_lifetime_scope` locals are
		// never present in the status maps and must not be flagged as "uninitialized".
		auto is_tracked = [&](MIRLocalRef local) {
			return local->scope.has_value() && local->scope.value() != fun.no_lifetime_scope
			    && not local->lifetime_flags.contains(LifetimeFlag::NoUseAfterFreeValidation);
		};

		const auto& block_in = args.liveness.block_in_liveness;

		auto check_instr = [&](const Instruction& instr, LocalStatusMap& map) {
			// Reads are validated against the state *before* the instruction executes, so a local
			// that is moved by this very instruction is still considered alive when read here.
			for (auto local: instructionReads(instr)) {
				if (not is_tracked(local)) continue;

				auto status = map.atMaybeCopy(local->id);
				if (not status.has_value() || *status != Status::Alive) {
					const char* title = !status.has_value()      ? "Use of an uninitialized value."
					                  : *status == Status::Moved ? "Use of a moved value."
					                                             : "Use of a possibly-moved value.";

					ctx.logInt(makeBox<dia_int::PlaceholderError>(
						title,
						base::strConcat(
							"The variable `",
							local->getName(),
							!status.has_value()        ? "` is used before it is initialized."
							: *status == Status::Moved ? "` is used after it has been moved out of."
													   : "` may have been moved out of on some "
					                                     "control-flow paths reaching "
														 "this use."
						)
					));
				}
			}

			updateLivenessMapByInstr(map, instr);
		};

		for (auto block_id: fun.block_order) {
			auto in = block_in.atMaybe(block_id);
			if (not in) continue;  // unreachable block, not part of the fixpoint result.

			LocalStatusMap map = *in.value();
			for (const auto& instr: fun.blocks.at(block_id)->instructions) check_instr(instr, map);
			check_instr(fun.blocks.at(block_id)->terminator, map);
		}
	}
}
