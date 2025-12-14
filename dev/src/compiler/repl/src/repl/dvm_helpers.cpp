#include "dvm_helpers.hpp"

#include <driver_private/backend_operations/compile_dvm.hpp>
#include <driver_private/operations.hpp>

#include <vm/api/vm.hpp>

namespace compiler::repl {

	std::expected<void, std::string> compileAndLoad(
		query::Context& ctx, const helios::HOUTUnit& hout_unit, vm::PID pid
	) {
		auto lir_data
			= driver::compileHOUTUnitToLIRModuleData(ctx, &hout_unit, base::StrID("repl_module"));
		auto dvm_code_collection = driver::compileLIRModuleToDVM(lir_data);

		return vm::api::loadCode(pid, dvm_code_collection).transform_error(vm::api::errorToString);
	}

	std::expected<ExpressionResult, std::string> executeExpression(
		vm::PID pid, const std::string& func_name, const tsh::SymbolType<>& return_type
	) {
		return vm::api::runFunction(pid, func_name, {})
		    .and_then([&] { return vm::api::join(pid); })
		    .and_then([&] { return vm::api::getExitValue(pid); })
		    .transform_error(vm::api::errorToString)
		    .and_then(
				[&return_type](Ref<vm::VmValue> exit_value
		        ) -> std::expected<ExpressionResult, std::string> {
					auto        type_str = return_type.toString();
					std::string result_str;

					if (type_str == "i32")
						result_str = std::to_string(exit_value->readBytes<i32>());
					else if (type_str == "i64")
						result_str = std::to_string(exit_value->readBytes<i64>());
					else if (type_str == "f32")
						result_str = std::to_string(exit_value->readBytes<float>());
					else if (type_str == "f64")
						result_str = std::to_string(exit_value->readBytes<double>());
					else if (type_str == "bool")
						result_str = exit_value->readBytes<bool>() ? "true" : "false";
					else
						return std::unexpected("Unsupported return type for REPL: " + type_str);

					return ExpressionResult{ .result_string = result_str };
				}
			);
	}

}  // namespace compiler::repl
