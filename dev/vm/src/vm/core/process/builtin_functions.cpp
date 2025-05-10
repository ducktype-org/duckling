#include "builtin_functions.hpp"

#include <base/exceptions.hpp>
#include <base/int_conv.hpp>
#include <base/macros/for_each.hpp>

#include <vm/bytecode/builtin_types.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/opcode_definitions.hpp>
#include <vm/core/process/builtin_functions.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/vmthread.hpp>
#include <vm/core/thread/vmvalue.hpp>

#include <type_traits>

namespace vm::builtins {

	namespace {
		template<class Ret, class... FunArgs, std::size_t... Is>
		base::Optional<VmValue>
			callUnpackArgsImpl(Ret (*function)(VMThread&, FunArgs...), TypeCRef vm_return_type, VMThread& thread, const std::vector<VmValue>& args, std::index_sequence<Is...>) {
			if (std::is_void_v<Ret>) {
				function(thread, args[Is].interpret<FunArgs>()...);
				return {};
			}
			auto value = function(thread, args[Is].interpret<FunArgs>()...);
			CORE_ASSERT(sizeof(value) == vm_return_type->getSize(), "Type sizes do not match");
			// @note THIS ASSUMES MATCHING ENDIANNESS
			return VmValue(vm_return_type, reinterpret_cast<byte*>(&value));
		}

		/**
		 * @brief Helper function that unpacks the arguments from the vector,
		 * converts them to the required types and calls the function.
		 * Throws an exception if the types do not match.
		 *
		 * Similar to python '*' operator:
		 * @code
		 * args = [1, 2, 4]
		 * f(*args)
		 *
		 * It checks if the size of the args vector is the same as the number of parameters.
		 * It checks if each variant type is the same as the function parameter type.
		 *
		 * @note alternative would be to pass the vector of Values to each handler and perform the
		 * `std::get` on variants inside each handler. Maybe if copying the values in calls would
		 * be expensive we could go back to that approach.
		 */
		template<class Ret, class... FunArgs>
		base::Optional<VmValue> callUnpackArgs(
			Ret (*function)(VMThread&, FunArgs...),
			TypeCRef                    vm_return_type,
			VMThread&                   thread,
			const std::vector<VmValue>& args
		) {
			if (sizeof...(FunArgs) != args.size()) CORE_PANIC("Argument number mismatch!");
			return callUnpackArgsImpl(
				function, vm_return_type, thread, args, std::index_sequence_for<FunArgs...>{}
			);
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

	base::Optional<VmValue> callBuiltinFunction(
		BuiltinFunctionID           id,
		TypeCRef                    builtin_func_type,
		VMThread&                   thread,
		const std::vector<VmValue>& arguments
	) {
		switch (id) {
#define CASE_FUNC(ID_NAME)                       \
	case BuiltinFunctionID::ID_NAME: {           \
		return callUnpackArgs(                   \
			FunctionHandlers::builtin##ID_NAME,  \
			*builtin_func_type->getResultType(), \
			thread,                              \
			arguments                            \
		);                                       \
	}

			FOR_EACH(CASE_FUNC, InputI64, OutputI64)

		default:
			CORE_PANIC("Invalid builtin function ID");
		}
	}

	auto getBuiltinFunctionTypes()
		-> CRef<std::unordered_map<BuiltinFunctionID, code::FunctionType>> {
		static const std::unordered_map<BuiltinFunctionID, code::FunctionType> map{
			{ BuiltinFunctionID::InputI64,
			  code::FunctionType(base::StrID("builtin_input_i64"), {}, base::StrID("i64")) },
			{ BuiltinFunctionID::OutputI64,
			  code::FunctionType(
				  base::StrID("builtin_output_i64"), { base::StrID("i64") }, base::StrID("i64")
			  ) }
		};

		return &map;
	}

	base::Optional<BuiltinFunctionID> getBuiltinFunctionID(base::StrID name) {
		// Lazy initialization of the "builtin name -> id" map.
		static const std::unordered_map<base::StrID, BuiltinFunctionID> builtin_function_indices
			= [] {
				  std::unordered_map<base::StrID, BuiltinFunctionID> indices;

				  for (const auto& func_tp: *getBuiltinFunctionTypes())
					  indices.emplace(func_tp.second.name, func_tp.first);
				  return indices;
			  }();

		auto it = builtin_function_indices.find(name);
		if (it != builtin_function_indices.end()) return it->second;
		return {};
	}

	CRef<code::CodeCollection> getStdlibModule() {
		static const code::CodeCollection builtin_module = []() {
			code::CodeCollection               module;
			code::builders::TypeContextBuilder types_builder(code::getBuiltinTypes());
			for (const auto& [id, func_type]: *getBuiltinFunctionTypes())
				types_builder.addType(func_type);
			module.types = types_builder.build().getTypes() | std::ranges::to<std::vector>();

			for (auto& [id, func_type]: *getBuiltinFunctionTypes()) {
				module.functions.emplace_back(code::Function{
					{},
					code::Identifier(func_type.name),
					{
						code::instructions::Op_call_builtin_func(
							vm::opargs::BuiltinFunctionName(func_type.name)
						),
						code::instructions::Op_ret{},
					} });
			}
			return module;
		}();

		return &builtin_module;
	}
}
