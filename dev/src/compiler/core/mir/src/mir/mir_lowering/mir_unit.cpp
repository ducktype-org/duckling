#pragma once

#include "mir_unit.hpp"

#include "mir_queries.hpp"

#include <base/extend_cpp/variant_match.hpp>

namespace compiler::mir {
	query::QResult<MIRUnit> lowerToMIRUnit(query::Context& ctx, CRef<helios::HOUTUnit> hout_unit) {
		MIRUnit unit_result;
		bool    is_failed = false;

		// Functions:
		for (const auto& hout_function: hout_unit->functions) {
			CRef<LowerToMIRFunctionResult> mir_function
				= ctx.query<mir::LowerToMIRFunction>({ hout_function });

			if (mir_function->hasFailed())
				is_failed = true;
			else
				unit_result.mir_functions.emplace_back(&mir_function->valueOrPanic());
		}

		// Globals:
		for (const auto& hout_global: hout_unit->glob_data) {
			// Discard information-less globals.
			if (not hout_global->type.getType().carriesInformation(ctx)) continue;

			variant_match(hout_global->value) {
				variant_case(helios::HOUTGlobalConst, ctv_initial_value) {
					CORE_ASSERT(
						hout_global->data_type == helios::HOUTGlobalDataType::Constant,
						"We assume currently HOUTGlobalConst <-> CTV initial value, change the "
						"code here if this ever changes"
					);

					unit_result.mir_globals.emplace_back(MIRGlobalData{
						.global = MIRGlobal{
							hout_global->helios_symbol,
							hout_global->type,
							MIRGlobal::Kind::Constant,
						},
						.initial_value = ctv_initial_value.value,
					});
				}
				variant_case(helios::HOUTGlobalVariable, hout_expr_initial_value) {
					CORE_ASSERT(
						hout_global->data_type == helios::HOUTGlobalDataType::Variable,
						"We assume currently HOUTGlobalVariable <-> CRef<mir::Function> initial "
						"value, change the code here if this ever changes"
					);

					CRef mir_ctor_function
						= ctx.query<mir::LowerGlobalDataToMIRCtor>({ hout_global });
					if (mir_ctor_function->hasFailed()) {
						is_failed = true;
						break;  // Note: this jump breaks only the variant_match
					}
					unit_result.mir_globals.emplace_back(MIRGlobalData{
						.global = MIRGlobal{
							hout_global->helios_symbol,
							hout_global->type,
							MIRGlobal::Kind::Variable,
						},
						.initial_value = &mir_ctor_function->valueOrPanic(),
					});
				}
				variant_default { CORE_UNREACHABLE(); }
			}
		}


		if (is_failed) return query::Failed();

		return unit_result;
	}
}
