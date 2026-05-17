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
#include <vm/bytecode/bytecode.hpp>

namespace compiler::repl {
	// Platform portability check: DVM assumes bool is 1 byte (stored as i8).
	// float and double sizes are already validated in base/types/floats.hpp.
	static_assert(sizeof(bool) == 1, "bool must be 1 byte for DVM compatibility");

	std::expected<vm::code::CodeCollection, std::string> compileHOUTUnitToDVMCode(
		query::Context&                  ctx,
		const helios::HOUTUnit&          hout_unit,
		std::string_view                 module_name,
		backend_vm::ReplLoweringContext& lowering_context
	) {
		auto active_ctx = lowering_context.getActiveContext();
		CORE_ASSERT(
			active_ctx.has_value() && active_ctx.value().get() == &ctx,
			"ReplLoweringContext's active query context must match the ctx parameter passed to "
			"compileHOUTUnitToDVMCode()"
		);

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

		const auto snapshot = lowering_context.captureLoweredEntitiesSnapshot();
		CORE_DEV_LOG(
			REPL,
			"Lowering context snapshot: types=",
			snapshot.loweredTypeCount(),
			", globals=",
			snapshot.loweredGlobalCount(),
			", functions=",
			snapshot.loweredFunctionCount(),
			", helper_functions=",
			snapshot.extraBytecodeFunctionCount(),
			"\n"
		);

		CRef lir_data
			= &ctx.query<driver::CompileHOUTUnitToLIRModuleData>({ &hout_unit, module_unique_name })
		           ->valueOrPanic();

		CORE_DEV_LOG(
			REPL,
			"LIR data contains ",
			lir_data->functions.size(),
			" functions and ",
			lir_data->globals.size(),
			" globals\n"
		);
		for (const auto& global: lir_data->globals) {
			CORE_DEV_LOG(REPL, "Global: ", global.lir_global.mangled_name.strView());
			if (global.global_ctor.has_value())
				CORE_DEV_LOG(
					REPL, "  Has ctor: ", global.global_ctor.value()->mangled_name.strView()
				);
			if (global.global_dtor.has_value())
				CORE_DEV_LOG(
					REPL, "  Has dtor: ", global.global_dtor.value()->mangled_name.strView()
				);
		}
		for (const auto& func: lir_data->functions)
			CORE_DEV_LOG(REPL, "Function: ", func->mangled_name.strView());

		// @TODO: #2246 check if we can avoid repeating the logic from compileLirToModuleData.
		// This is strictly connected to the loading dvm context.
		// We mimic the same idea as in compiling a single module,
		// but this time we append the new functions to the lowering context.
		for (const auto& global: lir_data->globals) {
			(void) lowering_context.lowerAndKeepLirGlobal(
				global.lir_global, global.global_ctor, global.global_dtor
			);
		}

		// Lower all functions
		for (const auto& lir_function: lir_data->functions) {
			CORE_DEV_LOG(REPL, "Lowering function: ", lir_function->mangled_name.strView(), "\n");
			(void) lowering_context.lowerAndKeepLirFunction(lir_function);
		}

		vm::code::CodeCollection new_code = lowering_context.collectNewCodeSince(snapshot);

		for (const auto& func: new_code.functions)
			CORE_DEV_LOG(REPL, "Adding lowered function: ", func.name.str, "\n");

		for (const auto& type: new_code.types)
			CORE_DEV_LOG(REPL, "Adding lowered type: ", vm::code::typeName(type), "\n");

		CORE_DEV_LOG(
			REPL,
			"Batch payload sizes: types=",
			new_code.types.size(),
			", globals=",
			new_code.global_data.size(),
			", functions=",
			new_code.functions.size(),
			"\n"
		);

		CORE_DEV_LOG(REPL, "[DEBUG] new_code contents to be loaded into VM:\n");
		CORE_DEV_LOG(REPL, "  Types (", new_code.types.size(), "):\n");
		for (const auto& type: new_code.types)
			CORE_DEV_LOG(REPL, "    Type: ", vm::code::typeName(type));
		CORE_DEV_LOG(REPL, "  Globals (", new_code.global_data.size(), "):\n");
		for (const auto& global: new_code.global_data) {
			CORE_DEV_LOG(REPL, "    Global: ", global.name.str, "\n");
			if (global.ctor_name.has_value())
				CORE_DEV_LOG(REPL, "      Ctor: ", global.ctor_name->str, "\n");
			if (global.dtor_name.has_value())
				CORE_DEV_LOG(REPL, "      Dtor: ", global.dtor_name->str, "\n");
		}
		CORE_DEV_LOG(REPL, "  Functions (", new_code.functions.size(), "):\n");
		for (const auto& func: new_code.functions)
			CORE_DEV_LOG(REPL, "    Function: ", func.name.str, "\n");
		CORE_DEV_LOG(REPL, "  External C Functions (", new_code.external_c_functions.size(), "):\n");
		for (const auto& ext_func: new_code.external_c_functions)
			CORE_DEV_LOG(REPL, "    ExtCFunction: ", ext_func.name.str, "\n");

		return new_code;
	}

	std::expected<void, std::string> compileAndLoad(
		query::Context&                  ctx,
		const helios::HOUTUnit&          hout_unit,
		std::string_view                 module_name,
		vm::PID                          pid,
		backend_vm::ReplLoweringContext& lowering_context
	) {
		return compileHOUTUnitToDVMCode(ctx, hout_unit, module_name, lowering_context)
		    .and_then([&](const vm::code::CodeCollection& code) {
				return vm::api::loadCode(pid, code).transform_error(vm::api::errorToString);
			});
	}

	// @TODO: #1817 This approach is hacky.
	// Instead of extracting the exit value manually based on type,
	// print would be called inside the DVM execution.
	std::expected<std::string, std::string> executeFunctionAndCaptureResult(
		vm::PID pid, std::string_view func_name, const tsh::SymbolType<>& return_type
	) {
		if (return_type.getRefKind() != tsh::ReferenceKind::Direct)
			return std::unexpected("Unsupported return type for REPL: " + return_type.toString());

		const auto&      raw_type_str = return_type.getType().toString();
		std::string_view type_view(raw_type_str);

		return vm::api::runFunction(pid, std::string(func_name), {})
		    .and_then([&](auto) { return vm::api::join(pid); })
		    .and_then([&] { return vm::api::getExitValue(pid); })
		    .transform_error(vm::api::errorToString)
		    .and_then(
				[type_view](vm::api::ExitValue exit_values
		        ) -> std::expected<std::string, std::string> {
					if (type_view == "()") {
						CORE_ASSERT(
							exit_values.empty(), "Expecting no return values for unit return type"
						);
						return { "" };
					}

					CORE_ASSERT(
						exit_values.size() == 1, "Expecting only one return value from the DVM"
					);
					auto& exit_value = exit_values.at(0);
					if (type_view == "i32")
						return std::to_string(exit_value->readBytes<i32>());
					else if (type_view == "i64")
						return std::to_string(exit_value->readBytes<i64>());
					else if (type_view == "f32")
						return std::to_string(exit_value->readBytes<f32>());
					else if (type_view == "f64")
						return std::to_string(exit_value->readBytes<f64>());
					else if (type_view == "bool")
						return exit_value->readBytes<bool>() ? "true" : "false";
					else
						return std::unexpected(
							"Unsupported return type for REPL: " + std::string(type_view)
						);
				}
			);
	}

}  // namespace compiler::repl
