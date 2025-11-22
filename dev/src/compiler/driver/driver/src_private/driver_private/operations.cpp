#include "operations.hpp"

#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_queries.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context.hpp>

namespace compiler::driver {


	LIRModuleData compileHOUTUnitToLIRModuleData(
		query::Context& ctx, base::CRef<compiler::helios::HOUTUnit> hout_unit, base::StrID module_id
	) {
		std::vector<LIRModuleGlobal> globals;
		globals.reserve(hout_unit->glob_data.size());

		for (const auto& hout_global: hout_unit->glob_data) {
			// Discard information-less globals.
			if (not hout_global.type.getType().carriesInformation(ctx)) continue;

			auto lir_global = lir::LIRGlobal::fromHOUT(ctx, hout_global);

			variant_match(hout_global.value) {
				variant_case(helios::HOUTGlobalVariable, var) {
					CRef mir_function
						= &ctx.query<mir::LowerGlobalDataToMIRCtor>({ hout_global })->value();
					auto lir_function = ctx.query<lir::LowerToLIRFunction>({ mir_function });
					globals.emplace_back(LIRModuleGlobal{
						.lir_global = lir_global,
						// @TODO: #929 add legit dtors when implemented
						.global_ctor = lir_function,
						.global_dtor = std::nullopt,
					});
				}
				variant_case(helios::HOUTGlobalConst, global_const) {
					// @future #1554 -- const ctors will probably be added here
					// Note: The CTV initial value for constants is already set in lir_global (by
					// the fromHOUT function used above). Backends should handle constant
					// initialization appropriately.
					globals.emplace_back(LIRModuleGlobal{
						.lir_global  = lir_global,
						.global_ctor = std::nullopt,
						.global_dtor = std::nullopt,
					});
				}
				variant_default {
					CORE_PANIC(base::strConcat(
						"Unexpected global data type in module: ",
						hout_global.original_name.strView()
					));
				}
			}
		}

		std::vector<CRef<lir::Function>> functions;
		functions.reserve(hout_unit->functions.size());

		for (const auto& hout_function: hout_unit->functions) {
			CRef mir_function = &ctx.query<mir::LowerToMIRFunction>({ hout_function })->value();
			auto lir_function = ctx.query<lir::LowerToLIRFunction>({ mir_function });
			functions.push_back(lir_function);
		}

		return LIRModuleData{
			.module_id = module_id,
			.functions = functions,
			.globals   = globals,
		};
	}
}
