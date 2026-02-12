#include "repl_dvm_helpers.hpp"

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

namespace compiler::repl {
	// Platform portability check: DVM assumes bool is 1 byte (stored as i8).
	// float and double sizes are already validated in base/types/floats.hpp.
	static_assert(sizeof(bool) == 1, "bool must be 1 byte for DVM compatibility");

	std::expected<void, std::string> compileAndLoad(
		query::Context& ctx, const helios::HOUTUnit& hout_unit, vm::PID pid
	) {
		CRef lir_data
			= &ctx.query<driver::CompileHOUTUnitToLIRModuleData>({ &hout_unit,
		                                                           base::StrID("repl_module") })
		           ->valueOrPanic();

		auto dvm_code_collection = driver::compileLIRModuleToDVM(lir_data);

		return vm::api::loadCode(pid, dvm_code_collection).transform_error(vm::api::errorToString);
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
			                      .and_then([&] { return vm::api::join(pid); })
			                      .transform_error(vm::api::errorToString);

			if (run_result.has_value())
				return std::string("");
			else
				return std::unexpected(run_result.error());
		}

		return vm::api::runFunction(pid, std::string(func_name), {})
		    .and_then([&] { return vm::api::join(pid); })
		    .and_then([&] { return vm::api::getExitValue(pid); })
		    .transform_error(vm::api::errorToString)
		    .and_then(
				[&type_str](Ref<vm::VmValue> exit_value) -> std::expected<std::string, std::string> {
					if (type_str == "i32")
						return std::to_string(exit_value->readBytes<i32>());
					else if (type_str == "i64")
						return std::to_string(exit_value->readBytes<i64>());
					// @TODO: #1795 DVM should also use f32 and f64.
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
