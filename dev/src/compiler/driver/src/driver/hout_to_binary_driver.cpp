#include "hout_to_binary_driver.hpp"

#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
// #include <query_framework/query_entry_point.hpp>
#include <query_framework/context.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include <base/str_utils.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

namespace compiler::driver {

	void HoutToBinaryDriver::compileHOUTUnit(
		query::Context&              ctx,
		base::CRef<helios::HOUTUnit> hout_unit,
		base::StrID                  module_id,
		artifacts::FileArtifact      output_artifact
	) {

		std::vector<BackendModuleGlobal> globals;
		globals.reserve(hout_unit->glob_data.size());

		for (const auto& hout_global: hout_unit->glob_data) {
			query::utils::withContextDo([&](query::Context& ctx) {
				auto lir_global = lir::LirGlobal::fromHOUT(ctx, hout_global);

				variant_match(hout_global.value) {
					variant_case(helios::HOUTGlobalVariable, var) {
						CRef mir_function
							= &ctx.query<mir::LowerGlobalDataToMirCtor>({ hout_global })->value();
						auto lir_function = ctx.query<lir::LowerToLirFunction>({ mir_function });
						globals.emplace_back(
							BackendModuleGlobal{
								.lir_global=lir_global,
								// @TODO: add legit dtors when implemented #929
								.global_ctor=lir_function,
								.global_dtor=std::nullopt
							}
						);
					}
					// @TODO: add ctors and dtors for Global Consts when implemented
					variant_default {
						globals.emplace_back(lir_global, std::nullopt, std::nullopt);
					}
				}
			});
		}

		std::vector<CRef<lir::Function>> functions;
		functions.reserve(hout_unit->functions.size());

		for (const auto& hout_function: hout_unit->functions) {
			CRef mir_function = &ctx.query<mir::LowerToMirFunction>({ hout_function })->value();
			auto lir_function = ctx.query<lir::LowerToLirFunction>({ mir_function });
			functions.push_back(lir_function);
		}

		BackendModuleData module_data{ .module_id = module_id, .functions = functions, .globals = globals };

		backend_driver->compileModule(ctx, module_data, std::move(output_artifact));
	}

	std::expected<RunOutput, std::string> HoutToBinaryDriver::run() {
		return backend_driver->run();
	}
}
