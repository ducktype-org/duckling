#include "operations.hpp"

#include <driver/module_flags/module_flags.hpp>
#include <driver_private/debug_artifacts.hpp>
#include <driver_private/lir_module_data.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_queries.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <hashing/component_hash.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <fstream>

namespace compiler::driver {
	void LIRModuleGlobal::debugPrint(query::Context& ctx, std::ostream& os) const {
		lir_global.debugPrint(ctx, os);

		if_opt_some(global_ctor, ctor) {
			os << "  Global constructor:\n";
			ctor->debugPrint(ctx, os);
		}
		if_opt_some(global_dtor, dtor) {
			os << "  Global destructor:\n";
			dtor->debugPrint(ctx, os);
		}
	}

	void LIRModuleData::debugPrint(query::Context& ctx, std::ostream& os) const {
		os << "LIRModuleData for module: " << module_id.strView() << "\n";
		os << "Functions:\n";
		for (const auto& func: functions) {
			func->debugPrint(ctx, os);
			os << "\n";
		}
		os << "Globals:\n";
		for (const auto& global: globals) {
			global.lir_global.debugPrint(ctx, os);
			os << "\n";
		}
	}

	std::ofstream getDebugDumpArtifact(base::StrID file_name) {
		auto          art = getDebugArtifactCollection()->fileArtifactAtOrNew(file_name);
		std::ofstream output_file(art.file.getFilePath().getPath(), std::ios::binary);
		return output_file;
	}

	struct IMPLEMENT_QUERY(CompileHOUTUnitToLIRModuleData, query::QResult<LIRModuleData>) {
		static auto provide(query::Context& ctx, CompileHOUTUnitToLIRModuleDataKey key) -> PResult {
			const auto& hout_unit   = *key.hout_unit.get();
			auto        module_name = key.module_name;

			if (driver::print_ir_options.print_hir) hout_unit.debugPrint(ctx, std::cout);
			if (driver::dump_ir_options.dump_hir) {
				auto ofstream = getDebugDumpArtifact(
					base::StrID(base::strConcat(module_name.strView(), ".hir"))
				);
				hout_unit.debugPrint(ctx, ofstream);
			}

			std::vector<CRef<mir::Function>> mir_functions;
			mir_functions.reserve(hout_unit.functions.size());

			for (const auto& hout_function: hout_unit.functions) {
				CRef mir_function
					= &ctx.query<mir::LowerToMIRFunction>({ hout_function })->valueOrThrow();
				mir_functions.push_back(mir_function);
			}

			if (driver::print_ir_options.print_mir) {
				std::ranges::for_each(mir_functions, [](CRef<mir::Function> mir_function) {
					mir_function->debugPrint(std::cerr);
				});
			}
			if (driver::dump_ir_options.dump_mir) {
				auto ofstream = getDebugDumpArtifact(
					base::StrID(base::strConcat(module_name.strView(), ".mir"))
				);
				std::ranges::for_each(mir_functions, [&](CRef<mir::Function> mir_function) {
					mir_function->debugPrint(ofstream);
				});
			}

			std::vector<CRef<lir::Function>> lir_functions;
			std::vector<LIRModuleGlobal>     globals;
			lir_functions.reserve(hout_unit.functions.size());
			globals.reserve(hout_unit.glob_data.size());

			for (const auto& mir_function: mir_functions) {
				CRef lir_function = ctx.query<lir::LowerToLIRFunction>({ mir_function });
				lir_functions.push_back(lir_function);
			}


			for (const auto& hout_global: hout_unit.glob_data) {
				// Discard information-less globals.
				if (not hout_global->type.getType().carriesInformation(ctx)) continue;

				auto lir_global = lir::LIRGlobal::fromHOUT(ctx, *hout_global);

				variant_match(hout_global->value) {
					variant_case(helios::HOUTGlobalVariable, var) {
						CRef mir_function
							= &ctx.query<mir::LowerGlobalDataToMIRCtor>({ hout_global })
						           ->valueOrThrow();
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
						// Note: The CTV initial value for constants is already set in lir_global
						// (by the fromHOUT function used above). Backends should handle constant
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
							hout_global->original_name.strView()
						));
					}
				}
			}


			auto lir_module = LIRModuleData{
				.module_id = module_name,
				.functions = lir_functions,
				.globals   = globals,
			};

			if (driver::print_ir_options.print_lir) lir_module.debugPrint(ctx, std::cout);
			if (driver::dump_ir_options.dump_lir) {
				auto ofstream = getDebugDumpArtifact(
					base::StrID(base::strConcat(module_name.strView(), ".lir"))
				);
				lir_module.debugPrint(ctx, ofstream);
			}

			return lir_module;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(CompileHOUTUnitToLIRModuleData);

	struct IMPLEMENT_QUERY(CompileToLIRModuleData, query::QResult<LIRModuleData>) {
		QUERY_AUTO_CACHE_CREF

		static auto provide(query::Context& ctx, frontend::ModuleID module_id) -> PResult {
			const auto& hout_unit = ctx.query<helios::QueryModuleHOUT>(module_id)->valueOrThrow();


			auto module_name
				= base::StrID(base::strConcat(
								  "module_",
								  compiler::frontend::ModuleTree::getPathComponentHash(module_id)
									  .hash.toStringHex()
				)
			                      .c_str());

			// we intentially make copy here, to keep the data in the
			// cache of this query
			return *ctx.query<CompileHOUTUnitToLIRModuleData>({ &hout_unit, module_name });
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(CompileToLIRModuleData);
}
