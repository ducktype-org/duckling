#include "operations.hpp"

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

namespace compiler::driver {


	struct IMPLEMENT_QUERY(CompileHOUTUnitToLIRModuleData, query::QResult<LIRModuleData>) {
		static auto provide(query::Context& ctx, CompileHOUTUnitToLIRModuleDataKey key) -> PResult {
			const auto& hout_unit   = *key.hout_unit.get();
			auto        module_name = key.module_name;

			std::vector<CRef<lir::Function>> functions;
			functions.reserve(hout_unit.functions.size());

			for (const auto& hout_function: hout_unit.functions) {
				CRef mir_function
					= &ctx.query<mir::LowerToMIRFunction>({ hout_function })->valueOrThrow();
				auto lir_function = ctx.query<lir::LowerToLIRFunction>({ mir_function });
				functions.push_back(lir_function);
			}

			std::vector<LIRModuleGlobal> globals;
			globals.reserve(hout_unit.glob_data.size());

			for (const auto& hout_global: hout_unit.glob_data) {
				// Discard information-less globals.
				if (not hout_global.type.getType().carriesInformation(ctx)) continue;

				auto lir_global = lir::LIRGlobal::fromHOUT(ctx, hout_global);

				variant_match(hout_global.value) {
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
							hout_global.original_name.strView()
						));
					}
				}
			}


			return LIRModuleData{
				.module_id = module_name,
				.functions = functions,
				.globals   = globals,
			};
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
