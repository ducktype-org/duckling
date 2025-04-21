#include "builtin_functions.hpp"

#include "base/exceptions.hpp"
#include "base/int_conv.hpp"

#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/vmthread.hpp>

namespace vm::builtins {
	Value FunctionHandlers::builtinInputI64Handler(VMThread& thread, const std::vector<Value>&) {
		thread.setProcessStatus(api::WaitingForInput{});
		auto return_value = thread.process.getIO().getInput<i64>(thread);
		thread.setProcessStatus(api::Running{});
		return return_value;
	}

	Value FunctionHandlers::builtinOutputI64Handler(
		VMThread& thread, const std::vector<Value>& arguments
	) {
		CORE_ASSERT(arguments.size() == 1, "Output builtin: expected one argument");

		std::string output;
		if (const i64* arg = std::get_if<i64>(&arguments[0])) {
			output = std::to_string(*arg) + "\n";
			thread.process.getIO().writeOutput(output);
		} else
			CORE_PANIC("Output builtin: expected i64 argument");

		return base::safeIntConv<i64>(output.size());
	}

	const std::array<Function, 2>& getBuiltinFunctions() {
		static const std::array<Function, 2> builtin_functions
			= { Function{
					.type
					= code::FunctionType(base::StrID("builtin_input_i64"), {}, base::StrID("i64")),
					.handler = FunctionHandlers::builtinInputI64Handler,
				},
			    Function{
					.type = code::FunctionType(
						base::StrID("builtin_output_i64"), { base::StrID("i64") }, base::StrID("i64")
					),
					.handler = FunctionHandlers::builtinOutputI64Handler,
				} };

		return builtin_functions;
	}

	base::Optional<usize> getBuiltinFunctionID(base::StrID name) {
		// Lazy initialization of the "builtin name -> id" map.
		static const std::unordered_map<base::StrID, usize> builtin_function_indices = []() {
			std::unordered_map<base::StrID, usize> indices;

			usize index = 0;
			for (const auto& func: getBuiltinFunctions()) indices.emplace(func.type.name, index++);
			return indices;
		}();

		auto it = builtin_function_indices.find(name);
		if (it != builtin_function_indices.end()) return it->second;
		return {};
	}

}
