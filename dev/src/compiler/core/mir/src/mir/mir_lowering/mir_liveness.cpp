#include "mir_liveness.hpp"

#include "mir_lifetimes.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include "base/collections/dynamic_bitset.hpp"

#include <logger/logger.hpp>
#include <query_framework/query_errors.hpp>

#include <algorithm>
#include <ostream>

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
		std::vector<dia_int::StablePosition>& dst, const std::vector<dia_int::StablePosition>& src
	) {
		for (const auto& pos: src)
			if (not std::ranges::contains(dst, pos)) dst.push_back(pos);
	}

	/**
	 * @brief Joins the status from two predecessor blocks, deciding what is the local status
	 * at the beginning of the successor block. If the value is not present in the map,
	 * it should be represented by an empty optional, it means that value is uninitialized.
	 *
	 * @warning Merging one uninitialized and one initialized gives unitinitialized by default.
	 * This is valid as it means that the destructor will be inserted on blocks with the initialized
	 * values, but at this point we don't have destructors inserted.
	 */
	base::Optional<MoveState> joinStatus(base::Optional<MoveState> a, base::Optional<MoveState> b) {
		// Missing on one of the incoming paths -> treated as uninitialized in the merge.
		if (not a.has_value() || not b.has_value()) return {};

		MoveStatus kind = (a->status == b->status) ? a->status : MoveStatus::MaybeMoved;

		MoveState result{ .status = kind, .move_sites = {} };
		// Collect the reaching move sites from every path that considers the value moved.
		if (kind != MoveStatus::Alive) {
			if (a->status != MoveStatus::Alive) mergeMoveSites(result.move_sites, a->move_sites);
			if (b->status != MoveStatus::Alive) mergeMoveSites(result.move_sites, b->move_sites);
		}
		return result;
	}

	/**
	 * @brief If the value is not present in one of the maps, then we assume that is it
	 * uninitialized in the merge, as we don't have the destructors inserted there yet.
	 */
	LocalMoveStateMap joinMaps(const LocalMoveStateMap& a, const LocalMoveStateMap& b) {
		LocalMoveStateMap result;
		for (const auto& [local, status]: a)
			if (auto joined = joinStatus(status, b.atMaybeCopy(local)))
				result.insertOrAssign(local, *joined);
		return result;
	}

	bool sameMap(const LocalMoveStateMap& a, const LocalMoveStateMap& b) {
		if (a.size() != b.size()) return false;
		for (const auto& [local, status]: a) {
			auto other = b.atMaybe(local);
			if (not other || *other.value() != status) return false;
		}
		return true;
	}

	void updateMoveStateMapByInstr(LocalMoveStateMap& map, const Instruction& instr) {
		for (const auto& flag: instr.flags) {
			switch (flag.flag) {
			case OperationFlag::Flag::Construct:
			case OperationFlag::Flag::Reinit:
				map.insertOrAssign(
					flag.local->id, MoveState{ .status = MoveStatus::Alive, .move_sites = {} }
				);
				break;
			case OperationFlag::Flag::Move: {
				// This instruction becomes the sole move reaching the value from here on.
				std::vector<dia_int::StablePosition> sites;
				if (instr.metadata.position.has_value())
					sites.push_back(instr.metadata.position.value());
				map.insertOrAssign(
					flag.local->id,
					MoveState{ .status = MoveStatus::Moved, .move_sites = std::move(sites) }
				);
				break;
			}
			default:
				break;
			}
		}
	}

	/**
	 * @brief Given LocalMoveStateMap valid at the start of the block,
	 * return the LocalMoveStateMap valid at the end of the block.
	 */
	LocalMoveStateMap transferBlock(const Block& block, LocalMoveStateMap map) {
		for (const auto& instr: block.instructions) updateMoveStateMapByInstr(map, instr);
		updateMoveStateMapByInstr(map, block.terminator);
		return map;
	}

	MoveStateData calculateGlobalInMoveStateMap(
		const Function& fun, const base::HashMap<BlockID, std::vector<BlockID>>& block_predecessors
	) {
		if (fun.block_order.empty()) return {};
		CORE_DEV_LOG(Compiler, "Calculating global move-state map for function `", fun.name, "`.\n");

		auto is_tracked = [&](MIRLocalRef local) {
			return local->scope.has_value() && local->scope.value() != fun.no_lifetime_scope
			    && not local->lifetime_flags.contains(LifetimeFlag::NoUseAfterFreeValidation);
		};

		// Parameters are alive on function entry.
		LocalMoveStateMap in_map_from_params;
		for (const auto& local: fun.local_list) {
			MIRLocalRef ref = base::Ref(&local);
			if (ref->parameter_index.has_value() && is_tracked(ref))
				in_map_from_params.insertOrAssign(
					ref->id, MoveState{ .status = MoveStatus::Alive, .move_sites = {} }
				);
		}


		base::HashMap<BlockID, LocalMoveStateMap> out_status;
		base::HashMap<BlockID, LocalMoveStateMap> in_status;

		auto entry = fun.block_order.front();

		auto compute_in = [&](BlockID block_id) {
			base::Optional<LocalMoveStateMap> acc;
			if (block_id == entry) acc = in_map_from_params;
			if (auto preds = block_predecessors.atMaybe(block_id))
				for (auto pred: *preds.value()) {
					auto pred_out = out_status.atMaybe(pred);
					if (not pred_out) continue;  // not yet reachable/processed
					acc = acc ? joinMaps(*acc, *pred_out.value()) : *pred_out.value();
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
			auto new_in   = compute_in(block_id);
			auto new_out  = transferBlock(*fun.blocks.at(block_id), new_in);

			auto prev_out = out_status.atMaybe(block_id);
			if (prev_out && sameMap(*prev_out.value(), new_out)) continue;
			out_status.insertOrAssign(block_id, std::move(new_out));
			in_status.insertOrAssign(block_id, std::move(new_in));

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
			    && not local->lifetime_flags.contains(LifetimeFlag::NoUseAfterFreeValidation);
		};

		const auto& block_in = args.move_states.block_in_move_state;
		bool        failed   = false;

		auto check_instr = [&](const Instruction& instr, LocalMoveStateMap& map) {
			// Reads are validated against the state *before* the instruction executes, so a local
			// that is moved by this very instruction is still considered alive when read here.
			for (auto local: instructionReads(instr)) {
				if (not is_tracked(local)) continue;

				auto state = map.atMaybeCopy(local->id);
				if (state.has_value() && state->status == MoveStatus::Alive) continue;

				const bool uninitialized = not state.has_value();

				auto title = base::strConcat(
					"The variable `",
					local->getName(),
					uninitialized ? "` is used before it is initialized."
					: state->status == MoveStatus::Moved
						? "` is used after it has been moved out of."
						: "` may have been moved out of on some "
						  "control-flow paths reaching this use."
				);

				// Anchor the error at the use site (placeholder until a real template exists).
				auto msg = makeBox<dia_int::PlaceholderError>(title, instr.metadata.position);

				// Point a note at every move instruction that reaches this use. If both
				// branches of an if/else move the value, both move sites are reported here.
				if (not uninitialized)
					for (const auto& site: state->move_sites)
						msg->addAttachedMessage(
							makeBox<dia_int::PlaceholderNote>("Value moved here.", site)
						);
				if (uninitialized) {
					if_opt_some(local->helios_id, sym_id) {
						if_opt_some(helios::maybeSymbolPst(sym_id), pst_elem) {
							msg->addAttachedMessage(
								makeBox<dia_int::PlaceholderNote>(
									"Variable declared here.",
									pst_elem.unlock(ctx)->getStablePosition()
								)
							);
						}
					}
				}

				ctx.logInt(std::move(msg));
				failed = true;
			}
			for (auto& local: instructionReinitDirectWrites(instr)) {
				if (not is_tracked(local)) continue;
				auto state = map.atMaybeCopy(local->id);
				if (state.has_value()) continue;
				// State is uninitialized
				auto msg = makeBox<dia_int::PlaceholderError>(
					base::strConcat(
						"The variable `", local->getName(), "` is used before it is initialized."
					),
					instr.metadata.position
				);
				if_opt_some(local->helios_id, sym_id) {
					if_opt_some(helios::maybeSymbolPst(sym_id), pst_elem) {
						msg->addAttachedMessage(
							makeBox<dia_int::PlaceholderNote>(
								"Variable declared here.", pst_elem.unlock(ctx)->getStablePosition()
							)
						);
					}
				}
				ctx.logInt(std::move(msg));
				failed = true;
			}

			updateMoveStateMapByInstr(map, instr);
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

	void MoveStateData::debugPrint(std::ostream& out) {
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

		out << "MoveStateData (in-status per block):\n";
		for (const auto& [block_id, map]: block_in_move_state) {
			out << "  block " << block_id.asInt() << ":\n";
			for (const auto& [local, state]: map) {
				out << "    local " << local.asInt() << " -> " << status_name(state.status);
				if (not state.move_sites.empty())
					out << " (moved at " << state.move_sites.size() << " site(s))";
				out << "\n";
			}
		}
	}

	// =========================== LIVENESS ANALYSIS ===========================

	struct BlockUseDef {
		base::DynamicBitset use;
		base::DynamicBitset def;
	};

	struct BlockLiveness {
		base::DynamicBitset live_in;
		base::DynamicBitset live_out;
	};

	base::HashMap<BlockID, BlockLiveness> calculateLivenessMap(
		const Function& fun, const LifetimePassArgs& pass_args
	) {
		base::HashMap<BlockID, BlockLiveness> result;
		base::HashMap<BlockID, BlockUseDef>   blocks_info;

		auto max_local_id = std::ranges::max_element(
								fun.local_list, std::ranges::greater{}, [](const MIRLocal& local) {
									return local.id.asInt();
								}
		)->id.asInt();
		auto get_local_bitset
			= [&] -> base::DynamicBitset { return base::DynamicBitset(max_local_id); };

		for (auto block_id: fun.block_order) {
			auto         block = fun.blocks.at(block_id);
			BlockUseDef& block_info
				= blocks_info
			          .put(
						  block_id,
						  BlockUseDef{ .use = get_local_bitset(), .def = get_local_bitset() }
					  )
			          .first->second;
			for (auto& instr: block->instructions)
				for (auto& flag: instr.flags) {
			}}
	}

	void AddMoves::run(query::Context&, Function&, const LifetimePassArgs&) {
		// calculateLivenessMap
		// go through the globcks from outside and add move when instruction is using a tempoarary
	}
}
