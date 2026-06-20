#include "../mir_structure/mir_structure.hpp"
#include "errors.hpp"
#include "mir_lifetimes.hpp"

#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/symbols/symbol_id_utils.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>

#include <algorithm>

namespace compiler::mir {

	class BlockWorklist {
		std::vector<bool>    present;
		std::vector<BlockID> stack;

	public:
		BlockWorklist(usize blocks): present(blocks, false) {}

		void pushBack(BlockID id) {
			if (not present.at(id.asInt())) {
				present.at(id.asInt()) = true;
				stack.push_back(id);
			}
		}

		BlockID pop() {
			auto id = stack.back();
			stack.pop_back();
			present.at(id.asInt()) = false;
			return id;
		}

		bool empty() { return stack.empty(); }
	};

	struct Transfer {
		std::vector<MIRLocalRef> moved_inside;
		std::vector<MIRLocalRef> created_inside;
	};

	Transfer computeTransfer(const Block& block) {
		Transfer transfer;
		for (const auto& stmt: block.instructions) {
			for (auto& flag: stmt.flags) {
				switch (flag.flag) {
				case OperationFlag::Flag::Move:
					transfer.moved_inside.push_back(flag.local);
					break;
				case OperationFlag::Flag::Construct:
					transfer.created_inside.push_back(flag.local);
					break;
				default:
					break;
				}
			}
		}
		for (auto& flag: block.terminator.flags) {
			switch (flag.flag) {
			case OperationFlag::Flag::Move:
				transfer.moved_inside.push_back(flag.local);
				break;
			case OperationFlag::Flag::Construct:
				transfer.created_inside.push_back(flag.local);
				break;
			default:
				break;
			}
		}
		return transfer;
	}

	enum class Status { Alive, Moved, MaybeMoved };
	using LocalStatusMap = base::HashMap<MIRLocalRef, Status>;

	void applyTransfer(const Transfer& transfer, LocalStatusMap& in_map, LocalStatusMap& out_map) {
		out_map = in_map;
		for (auto& local: transfer.created_inside) out_map.insertOrAssign(local, Status::Alive);
		for (auto& local: transfer.moved_inside) {
			if (out_map.contains(local)) {
				if (out_map.at(local) == Status::Alive)
					out_map.insertOrAssign(local, Status::Moved);
				else if (out_map.at(local) == Status::Moved)
					out_map.insertOrAssign(local, Status::MaybeMoved);
			} else {
				out_map.insertOrAssign(local, Status::MaybeMoved);
			}
		}
	}

	void calculateInputMapFromPredecessors(
		const std::vector<LocalStatusMap>& predecessors_out_maps, LocalStatusMap& result
	) {
		if (predecessors_out_maps.empty()) return;

		result = predecessors_out_maps.front();
		for (usize i = 1; i < predecessors_out_maps.size(); i++) {
			for (const auto& [local, status]: predecessors_out_maps.at(i)) {
				if (auto result_status_opt = result.atMaybe(local)) {
					auto result_status = *result_status_opt.value();
					switch (result_status) {
					case Status::Alive:
						switch (status) {
						case Status::Moved:
							result.insertOrAssign(local, Status::MaybeMoved);
							break;
						case Status::MaybeMoved:
							result.insertOrAssign(local, Status::MaybeMoved);
							break;
						case Status::Alive:
							// We do nothing
							break;
						}
						break;
					case Status::Moved:
						switch (status) {
						case Status::Alive:
							result.insertOrAssign(local, Status::MaybeMoved);
							break;
						case Status::MaybeMoved:
							result.insertOrAssign(local, Status::MaybeMoved);
							break;
						case Status::Moved:
							break;
						}
						break;
					case Status::MaybeMoved:
						// We do nothing as we won't change this
						break;
					}
				} else {
					result.erase(local);
				}
			}
		}
	}

	base::OkBad validateMoves(query::Context&, const Function& fun) {
		base::HashMap<BlockID, LocalStatusMap> in_local_status;
		base::HashMap<BlockID, LocalStatusMap> out_local_status;
		BlockWorklist                          worklist(fun.blocks.size());
		base::HashMap<BlockID, Transfer>       block_transfer_functions;

		for (const auto& block: fun.blocks)  // Fill with blocks.
			block_transfer_functions.put(block.key, computeTransfer(block.value));

		worklist.pushBack(fun.block_order.front());
		in_local_status.put(fun.block_order.front()) = {};

		while (not worklist.empty()) {
			auto        block_id  = worklist.pop();
			CRef<Block> block     = fun.blocks.at(block_id);
			auto&       in_status = in_local_status.at(block_id);
			auto&       transfer  = block_transfer_functions.at(block_id);
		}
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
