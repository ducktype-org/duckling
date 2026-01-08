#include "dvm_helpers.hpp"

#include <driver_private/backend_operations/compile_dvm.hpp>
#include <driver_private/lir_module_data.hpp>
#include <driver_private/operations.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_queries.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <string_id/string_id.hpp>

#include <vm/api/vm.hpp>

#include <optional>

namespace compiler::repl {
	// Platform portability check: DVM assumes bool is 1 byte (stored as i8).
	// float and double sizes are already validated in base/types/floats.hpp.
	static_assert(sizeof(bool) == 1, "bool must be 1 byte for DVM compatibility");

	driver::LIRModuleData compileHOUTUnitToLIRModuleData(
		query::Context& ctx, base::CRef<compiler::helios::HOUTUnit> hout_unit, base::StrID module_id
	) {
		std::vector<driver::LIRModuleGlobal> globals;
		globals.reserve(hout_unit->glob_data.size());

		for (const auto& hout_global: hout_unit->glob_data) {
			// Discard information-less globals.
			if (not hout_global.type.getType().carriesInformation(ctx)) continue;

			auto lir_global = lir::LIRGlobal::fromHOUT(ctx, hout_global);

			variant_match(hout_global.value) {
				variant_case(helios::HOUTGlobalVariable, var) {
					CRef mir_function
						= &ctx.query<mir::LowerGlobalDataToMIRCtor>({ hout_global })->valueOrThrow();
					auto lir_function = ctx.query<lir::LowerToLIRFunction>({ mir_function });
					globals.emplace_back(
						driver::LIRModuleGlobal{
							.lir_global = lir_global,
							// @TODO: #929 add legit dtors when implemented
							.global_ctor = lir_function,
							.global_dtor = std::nullopt,
						}
					);
				}
				variant_case(helios::HOUTGlobalConst, global_const) {
					// @future #1554 -- const ctors will probably be added here
					// Note: The CTV initial value for constants is already set in lir_global (by
					// the fromHOUT function used above). Backends should handle constant
					// initialization appropriately.
					globals.emplace_back(
						driver::LIRModuleGlobal{
							.lir_global  = lir_global,
							.global_ctor = std::nullopt,
							.global_dtor = std::nullopt,
						}
					);
				}
				variant_default {
					CORE_PANIC(
						base::strConcat(
							"Unexpected global data type in module: ",
							hout_global.original_name.strView()
						)
					);
				}
			}
		}

		std::vector<CRef<lir::Function>> functions;
		functions.reserve(hout_unit->functions.size());

		for (const auto& hout_function: hout_unit->functions) {
			CRef mir_function
				= &ctx.query<mir::LowerToMIRFunction>({ hout_function })->valueOrThrow();
			auto lir_function = ctx.query<lir::LowerToLIRFunction>({ mir_function });
			functions.push_back(lir_function);
		}

		return driver::LIRModuleData{
			.module_id = module_id,
			.functions = functions,
			.globals   = globals,
		};
	}

	std::expected<void, std::string> compileAndLoad(
		query::Context& ctx, const helios::HOUTUnit& hout_unit, vm::PID pid
	) {
		auto lir_data = compileHOUTUnitToLIRModuleData(ctx, &hout_unit, base::StrID("repl_module"));
		auto dvm_code_collection = driver::compileLIRModuleToDVM(lir_data);

		return vm::api::loadCode(pid, dvm_code_collection).transform_error(vm::api::errorToString);
	}

	std::expected<ExpressionResult, std::string> executeExpression(
		vm::PID pid, const std::string& func_name, const tsh::SymbolType<>& return_type
	) {
		auto type_str = return_type.toString();

		if (type_str == "void") {
			auto run_result = vm::api::runFunction(pid, func_name, {})
			                      .and_then([&] { return vm::api::join(pid); })
			                      .transform_error(vm::api::errorToString);

			if (run_result.has_value())
				return ExpressionResult{ .result_string = "" };
			else
				return std::unexpected(run_result.error());
		}

		return vm::api::runFunction(pid, func_name, {})
		    .and_then([&] { return vm::api::join(pid); })
		    .and_then([&] { return vm::api::getExitValue(pid); })
		    .transform_error(vm::api::errorToString)
		    .and_then(
				[&type_str](
					Ref<vm::VmValue> exit_value
				) -> std::expected<ExpressionResult, std::string> {
					std::string result_str;

					if (type_str == "i32")
						result_str = std::to_string(exit_value->readBytes<i32>());
					else if (type_str == "i64")
						result_str = std::to_string(exit_value->readBytes<i64>());
					else if (type_str == "f32")
						result_str = std::to_string(exit_value->readBytes<f32>());
					else if (type_str == "f64")
						result_str = std::to_string(exit_value->readBytes<f64>());
					else if (type_str == "bool")
						result_str = exit_value->readBytes<bool>() ? "true" : "false";
					else
						return std::unexpected("Unsupported return type for REPL: " + type_str);

					return ExpressionResult{ .result_string = result_str };
				}
			);
	}

}  // namespace compiler::repl
