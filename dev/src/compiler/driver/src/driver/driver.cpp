#include "driver.hpp"

#include "dvm_driver.hpp"
#include "llvm_driver.hpp"

#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include <base/str_utils.hpp>
#include <base/string_id.hpp>

namespace compiler::driver {

	Box<BackendDriver> createBackendDriver(CRef<Options> options) {
		switch (options->backend_type) {
		case BackendType::LLVM:
			return base::makeBox<LLVMDriver>(options);
		case BackendType::DVM:
			return base::makeBox<DVMDriver>(options);
		default:
			CORE_PANIC("Wrong enum value");
		}
	}

	void Driver::compileHOUTUnit(base::CRef<helios::HOUTUnit> hout_unit, base::StrID module_id) {
		std::vector<CRef<lir::Function>> functions;
		std::vector<std::tuple<
			lir::LirGlobal,
			base::Optional<CRef<lir::Function>>,
			base::Optional<CRef<lir::Function>>>>
			globals;
		globals.reserve(hout_unit->glob_data.size());
		functions.reserve(hout_unit->functions.size());

		for (const auto& hout_global: hout_unit->glob_data) {
			query::utils::withContextDo([&](query::Context& ctx) {
				auto lir_global = lir::LirGlobal::fromHOUT(ctx, hout_global);

				variant_match(hout_global.value) {
					variant_case(helios::HOUTGlobalVariable, var) {
						CRef mir_function
							= &ctx.query<mir::LowerGlobalDataToMirCtor>({ hout_global })->value();
						auto lir_function = ctx.query<lir::LowerToLirFunction>({ mir_function });
						globals.emplace_back(
							lir_global,
							// @TODO: add legit dtors when implemented
							lir_function,
							std::nullopt
						);
					}
					// @TODO: add ctors and dtors for Global Consts when implemented
					variant_default {
						globals.emplace_back(lir_global, std::nullopt, std::nullopt);
					}
				}
			});
		}

		for (const auto& hout_function: hout_unit->functions) {
			CRef mir_function
				= &query::entryPoint<mir::LowerToMirFunction>({ hout_function })->value();
			auto lir_function = query::entryPoint<lir::LowerToLirFunction>({ mir_function });
			functions.push_back(lir_function);
		}

		//@TODO: add dtors when implemented
		BackendModuleData module_data{ .module_id = module_id,
			                           .functions = functions,
			                           .globals   = globals };

		query::utils::withContextDo([&](query::Context& ctx) {
			backend_driver->compileModule(ctx, module_data);
		});
	}

	void Driver::link(base::StrID output_file) { backend_driver->link(output_file); }

	std::expected<RunOutput, std::string> Driver::run() { return backend_driver->run(); }
}
