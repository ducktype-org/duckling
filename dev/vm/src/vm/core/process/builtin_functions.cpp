#include "builtin_functions.hpp"

#include <base/exceptions.hpp>
#include <base/int_conv.hpp>

#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/vmthread.hpp>

namespace vm::builtins {
	namespace {
		template<class Ret, class... FunArgs, std::size_t... Is>
		Value callUnpackArgsImpl(Ret (*function)(VMThread&, FunArgs...), VMThread& thread, const std::vector<Value>& args, std::index_sequence<Is...>) {
			if constexpr (std::is_void_v<Ret>) {
				function(thread, std::get<FunArgs>(args[Is])...);
				return NoValue{};
			} else {
				return function(thread, std::get<FunArgs>(args[Is])...);
			}
		}

		/**
		 * @brief Helper function that unpacks the arguments from the vector,
		 * converts them to the required types and calls the function.
		 * Throws an exception if the types do not match.
		 *
		 * Similiar to python '*' operator:
		 * @code
		 * args = [1, 2, 4]
		 * f(*args)
		 *
		 * It checks if the size of the args vector is the same as the number of parameters.
		 * It checks if each variant type is the same as the function parameter type.
		 *
		 * @note alternative would be to pass the vector of Values to each handler and perform the
		 * `std::get` on variantes inside each handler. Maybe if copying the values in calls would
		 * be expensive we could go back to that approach.
		 */
		template<class Ret, class... FunArgs>
		Value callUnpackArgs(
			Ret (*function)(VMThread&, FunArgs...), VMThread& thread, const std::vector<Value> &args
		) {
			if (sizeof...(FunArgs) != args.size()) CORE_PANIC("Argument number mismatch!");
			return callUnpackArgsImpl(function, thread, args, std::index_sequence_for<FunArgs...>{});
		}
	}

	// ============================== BUILTIN IMPLEMENTATIONS ==============================

	i64 FunctionHandlers::builtinInputI64(VMThread& thread) {
		thread.setProcessStatus(api::WaitingForInput{});
		auto return_value = thread.process.getIO().getInput<i64>(thread);
		thread.setProcessStatus(api::Running{});
		return return_value;
	}

	i64 FunctionHandlers::builtinOutputI64(VMThread& thread, i64 arg) {
		std::string output;

		output = std::to_string(arg) + "\n";
		thread.process.getIO().writeOutput(output);

		return base::safeIntConv<i64>(output.size());
	}

	// ============================== BUILTIN DECLARATIONS AND ROUTER ==============================

	const std::array<code::FunctionType, 2>& getBuiltinFunctionTypes() {
		static const std::array<code::FunctionType, 2> function_types
			= { code::FunctionType(base::StrID("builtin_input_i64"), {}, base::StrID("i64")),
			    code::FunctionType(
					base::StrID("builtin_output_i64"), { base::StrID("i64") }, base::StrID("i64")
				) };

		return function_types;
	}

	/**
	 * @note The order and id should match the order in the getBuiltinFunctionTypes() array.
	 */
	Value callBuiltinFunction(usize id, VMThread& thread, const std::vector<Value>& arguments) {
		switch (id) {
		case 0:
			return callUnpackArgs(FunctionHandlers::builtinInputI64, thread, arguments);
		case 1:
			return callUnpackArgs(FunctionHandlers::builtinOutputI64, thread, arguments);
		default:
			CORE_PANIC("Invalid builtin function ID");
		}
	}

	// ============================== OTHER ==============================

	base::Optional<usize> getBuiltinFunctionID(base::StrID name) {
		// Lazy initialization of the "builtin name -> id" map.
		static const std::unordered_map<base::StrID, usize> builtin_function_indices = []() {
			std::unordered_map<base::StrID, usize> indices;

			usize index = 0;
			for (const auto& func_tp: getBuiltinFunctionTypes())
				indices.emplace(func_tp.name, index++);
			return indices;
		}();

		auto it = builtin_function_indices.find(name);
		if (it != builtin_function_indices.end()) return it->second;
		return {};
	}
}
