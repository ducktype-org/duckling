#include "../mir_structure/mir_structure.hpp"
#include "errors.hpp"
#include "mir_lifetimes.hpp"

#include <frontend/pst_parser/elements/includes/basic.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>

namespace compiler::mir {

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

							// If either variable is compiler-generated (no PST position),
							// skip the shadowing check — generated symbols are internal
							// implementation details and can't meaningfully shadow user code.
							// Fixes #2307: classes with fields named "__result" or similar
							// names that collide with generated constructor variables.
							if (!shadowing_pos.has_value() || !shadowed_pos.has_value()) continue;

							auto msg = makeBox<VariableShadowingError>(*shadowing_pos);
							msg->addAttachedMessage(
								makeBox<ShadowedDeclarationNote>(*shadowed_pos)
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

	base::OkBad validateFunction(query::Context& ctx, const Function& fun) {
		bool all_ok = validateShadowing(ctx, fun).isOk();
		return all_ok ? base::OK : base::BAD;
	}
}
