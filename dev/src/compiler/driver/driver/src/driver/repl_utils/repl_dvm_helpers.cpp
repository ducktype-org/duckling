#include "repl_dvm_helpers.hpp"

#include <backends/dvm/repl_lowering.hpp>
#include <driver_private/backend_operations/compile_dvm.hpp>
#include <driver_private/lir_module_data.hpp>
#include <driver_private/operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_queries.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <logger/logger.hpp>
#include <string_id/string_id.hpp>

#include <vm/api/vm.hpp>

namespace compiler::repl {
	std::expected<void, std::string> compileAndLoad(
		query::Context&                  ctx,
		const helios::HOUTUnit&          hout_unit,
		std::string_view                 module_name,
		vm::PID                          pid,
		backend_vm::ReplLoweringContext& lowering_context
	) {
		// Debug: Log HOUT functions before compilation
		for (const auto& hout_func: hout_unit.functions) {
			CORE_DEV_LOG(
				REPL,
				"HOUT function: ",
				hout_func->declaration->original_name.strView(),
				" (SymID: ",
				hout_func->declaration->original_symbol.queryUnstablePerfectHash(),
				")\n"
			);
		}

		auto module_unique_name = base::StrID(std::string(module_name.data(), module_name.size()));

		CORE_DEV_LOG(REPL, "Using module name: ", module_unique_name.strView(), "\n");

		CRef lir_data
			= &ctx.query<driver::CompileHOUTUnitToLIRModuleData>({ &hout_unit, module_unique_name })
		           ->valueOrPanic();

		vm::code::CodeCollection new_code;

		// We mimic the same idea as in compiling a single module,
		// but this time we append the new functions to the lowering context.
		for (const auto& global: lir_data->globals) {
			const auto& dvm_global = lowering_context.lowerAndKeepLirGlobal(
				global.lir_global, global.global_ctor, global.global_dtor
			);
			new_code.global_data.push_back(dvm_global);

			// lowerAndKeepLirGlobal internally lowers the ctor/dtor into the persistent
			// context, but we still need to explicitly add them to this batch for the DVM.
			if (global.global_ctor.has_value()) {
				const auto& ctor_func
					= lowering_context.lowerAndKeepLirFunction(global.global_ctor.value());
				new_code.functions.push_back(ctor_func);
			}
			if (global.global_dtor.has_value()) {
				const auto& dtor_func
					= lowering_context.lowerAndKeepLirFunction(global.global_dtor.value());
				new_code.functions.push_back(dtor_func);
			}
		}

		// Lower all functions
		for (const auto& lir_function: lir_data->functions) {
			CORE_DEV_LOG(REPL, "Lowering function: ", lir_function->mangled_name.strView(), "\n");
			const auto& dvm_func = lowering_context.lowerAndKeepLirFunction(lir_function);
			new_code.functions.push_back(dvm_func);
		}

		return vm::api::loadCode(pid, new_code).transform_error(vm::api::errorToString);
	}

	// @TODO: #1817 This approach is hacky.
	// Instead of extracting the exit value manually based on type,
	// print would be called inside the DVM execution.
	std::expected<std::string, std::string> executeFunctionAndCaptureResult(
		vm::PID pid, std::string_view func_name, const tsh::SymbolType<>& return_type
	) {
		auto type_str = return_type.toString();

		if (type_str == "void") {
			auto run_result = vm::api::runFunction(pid, std::string(func_name), {})
			                      .and_then([&](auto) { return vm::api::join(pid); })
			                      .transform_error(vm::api::errorToString);

			if (run_result.has_value())
				return std::string("");
			else
				return std::unexpected(run_result.error());
		}

		return vm::api::runFunction(pid, std::string(func_name), {})
		    .and_then([&](auto) { return vm::api::join(pid); })
		    .and_then([&] { return vm::api::getExitValue(pid); })
		    .transform_error(vm::api::errorToString)
		    .and_then(
				[&type_str](Ref<vm::VmValue> exit_value) -> std::expected<std::string, std::string> {
					if (type_str == "i32")
						return std::to_string(exit_value->readBytes<i32>());
					else if (type_str == "i64")
						return std::to_string(exit_value->readBytes<i64>());
					else if (type_str == "f32")
						return std::to_string(exit_value->readBytes<f32>());
					else if (type_str == "f64")
						return std::to_string(exit_value->readBytes<f64>());
					else if (type_str == "bool")
						return exit_value->readBytes<bool>() ? "true" : "false";
					else
						return std::unexpected("Unsupported return type for REPL: " + type_str);
				}
			);
	}

}  // namespace compiler::repl
