#pragma once

#include "mir_unit.hpp"
#include "mir_queries.hpp"

#include <base/extend_cpp/variant_match.hpp>

namespace compiler::mir {
	query::QResult<MIRUnit> lowerToMIRUnit(query::Context& ctx, CRef<helios::HOUTUnit> hout_unit) {
		// #2246 implement this
		
		MIRUnit unit_result;
		bool is_failed = false;

		// Functions:
		for (const auto& hout_function: hout_unit->functions) {
			CRef<LowerToMIRFunctionResult> mir_function
				= ctx.query<mir::LowerToMIRFunction>({ hout_function });

			if (mir_function->hasFailed()) {
				is_failed = true;
			}
			else {
				unit_result.mir_functions.emplace_back(&mir_function->valueOrPanic());
			}
		}

		// Globals:
		for (const auto& hout_global: hout_unit->glob_data) {
			// Discard information-less globals.
			if (not hout_global->type.getType().carriesInformation(ctx)) continue;

			auto helios_kind_to_mir_kind = [](helios::HOUTGlobalDataType type) -> MIRGlobalData::Kind {
				switch (type) {
					case helios::HOUTGlobalDataType::Constant: return MIRGlobalData::Kind::Constant;
					case helios::HOUTGlobalDataType::Variable: return MIRGlobalData::Kind::Variable;
					default: CORE_UNREACHABLE();
				}
			};

			variant_match(hout_global->value) {
				variant_case(helios::HOUTGlobalConst, ctv_initial_value) {
					unit_result.mir_globals.emplace_back(MIRGlobalData{
						.helios_id = hout_global->helios_symbol,
						.type = hout_global->type,
						.kind = helios_kind_to_mir_kind(hout_global->data_type),
						.initial_value = ctv_initial_value.value,
					});
				}
				variant_case(helios::HOUTGlobalVariable, hout_expr_initial_value) {
					CRef mir_ctor_function
							= ctx.query<mir::LowerGlobalDataToMIRCtor>({ hout_global });
					if (mir_ctor_function->hasFailed()) {
						is_failed = true;
						break; // Note: this jump breaks only the variant_match
					}
					unit_result.mir_globals.emplace_back(MIRGlobalData{
						.helios_id = hout_global->helios_symbol,
						.type = hout_global->type,
						.kind = helios_kind_to_mir_kind(hout_global->data_type),
						.initial_value = &mir_ctor_function->valueOrPanic(),
					});
				}
				variant_default {
					CORE_UNREACHABLE();
				}
			}

			
		}


		if (is_failed) {
			return query::Failed();
		}

		return unit_result;
	}
}
