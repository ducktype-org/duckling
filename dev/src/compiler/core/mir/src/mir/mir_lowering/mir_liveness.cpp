#include "mir_liveness.hpp"

#include "mir_lifetimes.hpp"

#include <frontend/pst_parser/lang_parser_element.hpp>
#include <mir/mir_lowering/mir_destructors.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <base/except/exceptions.hpp>

#include <diagnostic/placeholder.hpp>
#include <logger/logger.hpp>
#include <query_framework/query_errors.hpp>

#include <algorithm>
#include <ostream>
#include <sstream>

namespace compiler::mir {
	/**
	 * @brief Helper structure to efficiently add and remove
	 * elements from the stack.
	 */
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
			auto top = stack.back();
			stack.pop_back();
			on_stack.at(top.asInt()) = false;
			return top;
		}

		bool empty() { return stack.empty(); }
	};

	/**
	 * @brief Append @p src move sites to @p dst, skipping positions already present.
	 */
	void mergeMoveSites(
		std::vector<dia::StablePosition>& dst, const std::vector<dia::StablePosition>& src
	) {
		for (const auto& pos: src)
			if (not std::ranges::contains(dst, pos)) dst.push_back(pos);
	}

	/**
	 * @brief Joins the status from two predecessor blocks, deciding what is the local status
	 * at the beginning of the successor block. If the value is not present in the map,
	 * it should be represented by an empty optional, it means that value is uninitialized.
	 *
	 * Merging one uninitialized and one initialized path gives @ref MoveStatus::MaybeMoved with
	 * no move sites: the value is not usable after the join, but it may still own a resource on
	 * one of the paths, so the destructor has to be emitted conditionally (`DestructIf`).
	 */
	base::Optional<MoveState> joinStatus(
		base::Optional<MoveState> a, base::Optional<MoveState> b, ScopeRef into_scope
	) {
		CORE_ASSERT(
			a.empty() or b.empty() or a->local == b->local,
			"Joining move states of two different locals"
		);

		if (a.has_value() && not isAliveInScope(a->local, into_scope)) a = {};
		if (b.has_value() && not isAliveInScope(b->local, into_scope)) b = {};

		if (a.empty() and b.empty()) return {};
		if (a.empty() or b.empty()) {
			auto& state = a.empty() ? b.value() : a.value();
			return MoveState{
				.local      = state.local,
				.status     = MoveStatus::MaybeMoved,
				.move_sites = {},
			};
		}

		MoveStatus kind = (a->status == b->status) ? a->status : MoveStatus::MaybeMoved;

		MoveState result{ .local = a->local, .status = kind, .move_sites = {} };
		// Collect the reaching move sites from every path that considers the value moved.
		if (kind != MoveStatus::Alive) {
			if (a->status != MoveStatus::Alive) mergeMoveSites(result.move_sites, a->move_sites);
			if (b->status != MoveStatus::Alive) mergeMoveSites(result.move_sites, b->move_sites);
		}
		return result;
	}

	LocalMoveStateMap LocalMoveStateMap::join(
		const LocalMoveStateMap& a, const LocalMoveStateMap& b, ScopeRef into_scope
	) {
		LocalMoveStateMap result;
		for (const auto& [local, status]: a.map)
			if (auto joined = joinStatus(status, b.map.atMaybeCopy(local), into_scope))
				result.map.insertOrAssign(local, *joined);
		for (const auto& [local, status]: b.map)
			if (not a.map.contains(local))
				if (auto joined = joinStatus({}, status, into_scope))
					result.map.insertOrAssign(local, *joined);
		return result;
	}

	bool LocalMoveStateMap::hasSameDataFlowState(const LocalMoveStateMap& other) const {
		if (map.size() != other.map.size()) return false;
		for (const auto& [local, status]: map) {
			auto other_status = other.map.atMaybe(local);
			if (not other_status || *other_status.value() != status) return false;
		}
		return true;
	}

	void LocalMoveStateMap::updateMoveStateMapByInstr(
		const Instruction& instr, const LocalsByScopeMap& locals_by_scope
	) {
		// Mark as uninitialized the locals where the scope ends.
		if_opt_some(prev_instr_scope, prev_scope) {
			for (auto scope: getEndingScopes(prev_scope, instr.scope)) {
				auto locals = locals_by_scope.atMaybe(scope);
				if (locals.empty()) continue;
				for (auto local: *locals.value()) map.erase(local->id);
			}
		}

		for (const auto& flag: instr.flags) {
			switch (flag.flag) {
			case OperationFlag::Flag::Construct:
			case OperationFlag::Flag::Reinit:
				map.insertOrAssign(
					flag.local->id,
					MoveState{
						.local      = flag.local,
						.status     = MoveStatus::Alive,
						.move_sites = {},
					}
				);
				break;
			case OperationFlag::Flag::Move: {
				// This instruction becomes the sole move reaching the value from here on.
				std::vector<dia::StablePosition> sites;
				if (instr.metadata.position.has_value())
					sites.push_back(instr.metadata.position.value());
				map.insertOrAssign(
					flag.local->id,
					MoveState{
						.local      = flag.local,
						.status     = MoveStatus::Moved,
						.move_sites = std::move(sites),
					}
				);
				break;
			}
			default:
				break;
			}
		}
		prev_instr_scope = instr.scope;
	}

	/**
	 * @brief Given LocalMoveStateMap valid at the start of the block,
	 * return the LocalMoveStateMap valid at the end of the block.
	 */
	LocalMoveStateMap transferBlock(
		const Block& block, LocalMoveStateMap map, const LocalsByScopeMap& locals_by_scope
	) {
		for (const auto& instr: block.instructions)
			map.updateMoveStateMapByInstr(instr, locals_by_scope);
		map.updateMoveStateMapByInstr(block.terminator, locals_by_scope);
		return map;
	}

	MoveStateData MoveStateData::calculateGlobalInMoveStateMap(
		const Function&                                     fun,
		const base::HashMap<BlockID, std::vector<BlockID>>& block_predecessors,
		const LocalsByScopeMap&                             locals_by_scope
	) {
		if (fun.block_order.empty()) return {};
		CORE_DEV_LOG(Compiler, "Calculating global move-state map for function `", fun.name, "`.\n");

		auto is_tracked = [&](MIRLocalRef local) {
			return local->scope.has_value() && local->scope.value() != fun.no_lifetime_scope
			    && not local->lifetime_flags.contains(LifetimeFlag::NoMoveStatusValidation);
		};

		// Parameters are alive on function entry.
		LocalMoveStateMap in_map_from_params;
		for (const auto& local: fun.local_list) {
			MIRLocalRef ref = base::Ref(&local);
			if (ref->parameter_index.has_value() && is_tracked(ref))
				in_map_from_params.markAlive(ref);
		}


		base::HashMap<BlockID, LocalMoveStateMap> out_status;
		base::HashMap<BlockID, LocalMoveStateMap> in_status;

		auto entry = fun.block_order.front();

		auto compute_in = [&](CRef<Block> block) {
			base::Optional<LocalMoveStateMap> acc;
			if (block->id == entry) acc = in_map_from_params;
			if (auto preds = block_predecessors.atMaybe(block->id))
				for (auto pred: *preds.value()) {
					auto pred_out = out_status.atMaybe(pred);
					if (not pred_out) continue;  // not yet reachable/processed
					acc = acc
					        ? LocalMoveStateMap::join(*acc, *pred_out.value(), block->beginScope())
					        : *pred_out.value();
				}
			return acc ? std::move(*acc) : LocalMoveStateMap{};
		};

		// Worklist fixpoint, seeded from the entry so only reachable blocks are processed.
		// If the calculated out move-state map differs, we add the successors to the worklist
		// so they are recalculated.
		Worklist worklist{ std::ranges::max_element(fun.block_order)->asInt() + 1 };
		worklist.push(entry);
		while (not worklist.empty()) {
			auto block_id = worklist.pop();
			auto block    = fun.blocks.at(block_id);
			auto new_in   = compute_in(block);
			auto new_out  = transferBlock(*block, new_in, locals_by_scope);

			// The in-state is recorded even when the out-state did not change, because a block can
			// hide a changed in-state from its successors.
			// A block that reinitializes a local ends with it alive no matter what it
			// started with, and the in-state is what the subsequent passes look at.
			// For example:
			// ```
			// var a: Res = Res(13);
			// if (a.id == 13) {
			//     eat(move a);
			// }
			// a = Res(14);
			// ```
			// `a = Res(14)` should get a `DestructIf` inserted, after the first iteration the
			// `Reinit` flag will mark it as Alive, then in the second iteration when `a` is marked
			// as `MaybeMoved`, the `Reinit` will mark it as Alive again causing the outputs to not
			// change, but the `in_status` has been changed. Thus we update the in unconditionally
			// before ending the fixpoint loop.
			in_status.insertOrAssign(block_id, std::move(new_in));

			auto prev_out = out_status.atMaybe(block_id);
			if (prev_out && prev_out.value()->hasSameDataFlowState(new_out)) continue;
			out_status.insertOrAssign(block_id, std::move(new_out));

			for (auto succ: getTerminatorSuccessors(fun.blocks.at(block_id)->terminator))
				worklist.push(succ);
		}

		MoveStateData result{ .block_in_move_state = std::move(in_status) };

		CORE_DEV_LOG(Compiler, "Move-state info calculated.\n");
		IF_BUILD_TYPE_DEV({
			std::stringstream sstr;
			result.debugPrint(sstr);
			CORE_DEV_LOG(Compiler, sstr.str());
		})

		return result;
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
		if (instr.output && instr.output->hasProjections())
			collectPlaceReads(*instr.output, true, reads);
		return reads;
	}

	/**
	 * Get the vector of locals, to which the instruction writes (excluding those which are
	 * constructed by this instruction). Don't return locals, where the instruction also read in
	 * instructionReads. For example `a[1] = ...` both reads and writes to `a`, so it won't be
	 * returned. we only return direct writes, like `a = ...`.
	 *
	 * @note We don't return values with Construct flag in this instruction, as we want to catch the
	 * values used before construction.
	 */
	std::vector<MIRLocalRef> instructionReinitDirectWrites(const Instruction& instr) {
		// If we construct a result, then this is not a write
		for (auto& flag: instr.flags)
			if (flag.flag == OperationFlag::Flag::Construct) return {};
		// The output->hasProjections() doesn't matter, but if there were projections,
		// then the base would also be read and we would also detect
		if (instr.output && instr.output->isLocal() && not instr.output->hasProjections())
			return { instr.output->getBase<MIRLocalRef>() };
		return {};
	}

	void InvalidUseCheck::run(query::Context& ctx, Function& fun, const LifetimePassArgs& args) {
		if (fun.block_order.empty()) return;

		// Same tracking predicate as the move-state fixpoint: only locals that participate in
		// lifetime analysis are validated. Untracked temporaries / `no_lifetime_scope` locals are
		// never present in the status maps and must not be flagged as "uninitialized".
		auto is_tracked = [&](MIRLocalRef local) {
			return local->scope.has_value() && local->scope.value() != fun.no_lifetime_scope
			    && not local->lifetime_flags.contains(LifetimeFlag::NoMoveStatusValidation);
		};

		const auto& block_in = args.move_states.block_in_move_state;
		bool        failed   = false;

		auto check_instr = [&](const Instruction& instr, LocalMoveStateMap& map) {
			// Reads are validated against the state *before* the instruction executes, so a local
			// that is moved by this very instruction is still considered alive when read here.
			for (auto local: instructionReads(instr)) {
				if (not is_tracked(local)) continue;

				auto state = map.stateOf(local->id);
				if (state.has_value() && state.value()->status == MoveStatus::Alive) continue;

				const bool uninitialized = not state.has_value();
				// A `MaybeMoved` state with no reaching move sites does not come from a move:
				// it is a local that is simply uninitialized on some of the incoming paths.
				const bool maybe_uninitialized = not uninitialized
				                              && state.value()->status == MoveStatus::MaybeMoved
				                              && state.value()->move_sites.empty();

				auto title = base::strConcat(
					"The variable `",
					local->getName(),
					uninitialized         ? "` is used before it is initialized."
					: maybe_uninitialized ? "` is not initialized on some "
											"control-flow paths reaching this use."
					: state.value()->status == MoveStatus::Moved
						? "` is used after it has been moved out of."
						: "` may have been moved out of on some "
						  "control-flow paths reaching this use."
				);

				// Anchor the error at the use site (placeholder until a real template exists).
				auto msg = makeBox<dia::PlaceholderError>(title, instr.metadata.position);

				// Point a note at every move instruction that reaches this use. If both
				// branches of an if/else move the value, both move sites are reported here.
				if (not uninitialized)
					for (const auto& site: state.value()->move_sites)
						msg->addAttachedMessage(
							makeBox<dia::PlaceholderNote>("Value moved here.", site)
						);
				if (uninitialized || maybe_uninitialized) {
					if_opt_some(local->helios_id, sym_id) {
						if_opt_some(helios::maybeSymbolPst(sym_id), pst_elem) {
							msg->addAttachedMessage(makeBox<dia::PlaceholderNote>(
								"Variable declared here.", pst_elem.unlock(ctx)->getStablePosition()
							));
						}
					}
				}

				ctx.logInt(std::move(msg));
				failed = true;
			}
			for (auto& local: instructionReinitDirectWrites(instr)) {
				if (not is_tracked(local)) continue;
				auto state = map.stateOf(local->id);
				if (state.has_value()) continue;
				// State is uninitialized
				auto msg = makeBox<dia::PlaceholderError>(
					base::strConcat(
						"The variable `", local->getName(), "` is used before it is initialized."
					),
					instr.metadata.position
				);
				if_opt_some(local->helios_id, sym_id) {
					if_opt_some(helios::maybeSymbolPst(sym_id), pst_elem) {
						msg->addAttachedMessage(makeBox<dia::PlaceholderNote>(
							"Variable declared here.", pst_elem.unlock(ctx)->getStablePosition()
						));
					}
				}
				ctx.logInt(std::move(msg));
				failed = true;
			}

			map.updateMoveStateMapByInstr(instr, args.locals_by_scope);
		};

		for (auto block_id: fun.block_order) {
			auto in = block_in.atMaybe(block_id);
			if (not in) continue;  // unreachable block, not part of the fixpoint result.

			LocalMoveStateMap map = *in.value();
			for (const auto& instr: fun.blocks.at(block_id)->instructions) check_instr(instr, map);
			check_instr(fun.blocks.at(block_id)->terminator, map);
		}

		if (failed) query::throwFailed();
	}

	void LocalMoveStateMap::debugPrint(std::ostream& out) const {
		auto status_name = [](MoveStatus status) -> std::string_view {
			switch (status) {
			case MoveStatus::Alive:
				return "Alive";
			case MoveStatus::Moved:
				return "Moved";
			case MoveStatus::MaybeMoved:
				return "MaybeMoved";
			default:
				CORE_UNREACHABLE();
			}
		};

		for (const auto& [local, state]: map) {
			out << "    local " << local.asInt() << " -> " << status_name(state.status);
			if (not state.move_sites.empty())
				out << " (moved at " << state.move_sites.size() << " site(s))";
			out << "\n";
		}
	}

	void MoveStateData::debugPrint(std::ostream& out) {
		out << "MoveStateData (in-status per block):\n";
		for (const auto& [block_id, map]: block_in_move_state) {
			out << "  block " << block_id.asInt() << ":\n";
			map.debugPrint(out);
		}
	}
}
