// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
			if (local.lifetime_flags.contains(LifetimeFlag::NoShadowingValidation)) continue;

			// Generated locals cannot shadow user code and have no position of their own.
			if (not helios::maybeSymbolPst(*local.helios_id).has_value()) continue;

			auto name = helios::name(*local.helios_id);
			match_optional(named_locals.atMaybe(name)) {
				opt_some(prev_defs) {
					for (auto def: *prev_defs) {
						if (def->lifetime_flags.contains(LifetimeFlag::NoShadowingValidation))
							continue;
						auto lc_scope = lca(*def->scope, *local.scope);
						if (lc_scope == *def->scope || lc_scope == *local.scope) {
							// Since the LCA is one of the scopes, the other has to be contained in it.
							auto [shadowing, shadowed] = (lc_scope == *def->scope)
							                               ? std::tuple{ base::Ref(&local), def }
							                               : std::tuple{ def, base::Ref(&local) };

							auto pos_of = [&](auto local_ref) {
								return helios::maybeSymbolPst(local_ref->helios_id.value())
								    .value()
								    .unlock(ctx)
								    ->getStablePosition();
							};

							auto shadowing_pos = pos_of(shadowing);
							auto shadowed_pos  = pos_of(shadowed);

							auto msg = makeBox<VariableShadowingError>(shadowing_pos);
							msg->addAttachedMessage(makeBox<ShadowedDeclarationNote>(shadowed_pos));
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
