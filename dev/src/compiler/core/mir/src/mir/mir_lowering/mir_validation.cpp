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
	};

	struct Transfer {
		std::vector<LocalID> moved_inside;
		std::vector<LocalID> created_inside;
	};

	base::OkBad validateMoves(query::Context&, const Function& fun) {
		enum class Status { Alive, Moved, MaybeMoved };
		using LocalStatusMap = base::HashMap<LocalID, Status>;

		base::HashMap<BlockID, LocalStatusMap> in_local_status;
		base::HashMap<BlockID, LocalStatusMap> out_local_status;
		BlockWorklist                          worklist(fun.blocks.size());
		base::HashMap<BlockID, Transfer>       block_transfer_functions;

		for (const auto& block: fun.blocks)  // Fill with blocks.
		                                     // variables_status.emplace(block.key, LocalSet());
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
