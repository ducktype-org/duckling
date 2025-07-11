#include "operations.hpp"

// #include "backend_operations/compile_dvm.hpp"
// #include "backend_operations/compile_llvm.hpp"

#include <backends/llvm/llvm_backend.hpp>
#include <global_state/artifacts_location.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/context.hpp>

#include <base/variant.hpp>

namespace compiler::driver {


	LIRModuleData compileHOUTUnitToLIRModuleData(
		query::Context& ctx, base::CRef<compiler::helios::HOUTUnit> hout_unit, base::StrID module_id
	) {
		std::vector<LIRModuleGlobal> globals;
		globals.reserve(hout_unit->glob_data.size());

		for (const auto& hout_global: hout_unit->glob_data) {
			auto lir_global = lir::LirGlobal::fromHOUT(ctx, hout_global);

			variant_match(hout_global.value) {
				variant_case(helios::HOUTGlobalVariable, var) {
					CRef mir_function
						= &ctx.query<mir::LowerGlobalDataToMirCtor>({ hout_global })->value();
					auto lir_function = ctx.query<lir::LowerToLirFunction>({ mir_function });
					globals.emplace_back(LIRModuleGlobal{
						.lir_global = lir_global,
						// @TODO: add legit dtors when implemented #929
						.global_ctor = lir_function,
						.global_dtor = std::nullopt,
					});
				}
				// @TODO: add ctors and dtors for Global Consts when implemented
				variant_case(helios::HOUTGlobalConst, global_const) {
					CORE_PANIC(base::strConcat(
						"Creating ctors for constant variables is not implemented yet. "
						"Global constant: ",
						hout_global.original_name.strView()
					));
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
			CRef mir_function = &ctx.query<mir::LowerToMirFunction>({ hout_function })->value();
			auto lir_function = ctx.query<lir::LowerToLirFunction>({ mir_function });
			functions.push_back(lir_function);
		}

		return LIRModuleData{
			.module_id = module_id,
			.functions = functions,
			.globals   = globals,
		};
	}

	// void compileHOUTUnit(
	// 	query::Context&                        ctx,
	// 	base::CRef<compiler::helios::HOUTUnit> hout_unit,
	// 	base::StrID                            module_id,
	// 	const artifacts::FileArtifact&         output_artifact,
	// 	BackendType                            backend_type
	// ) {
	// 	auto module_data = compileHOUTUnitToLIRModuleData(ctx, hout_unit, module_id);

	// 	compileBackendModule(ctx, module_data, output_artifact, backend_type);
	// }

	// void compileBackendModule(
	// 	query::Context&                ctx,
	// 	const LIRModuleData&       module_data,
	// 	const artifacts::FileArtifact& output_artifact,
	// 	BackendType                    backend_type
	// ) {
	// 	switch (backend_type) {
	// 	case BackendType::LLVM: {
	// 		compileLIRModuleToLLVM(ctx, module_data, output_artifact);
	// 		break;
	// 	}
	// 	case BackendType::DVM: {
	// 		compileLIRModuleToDVM(ctx, module_data, output_artifact);
	// 		break;
	// 	}
	// 	default:
	// 		CORE_PANIC("Unsupported backend type for compilation");
	// 	}
	// }


}
