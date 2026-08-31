#include "../function_lowering_context.hpp"
#include "../program_lowering_context.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

#include <logger/logger.hpp>

namespace {
	using namespace compiler::backend_vm::internal;

	/**
	 * @brief Builds the FFI declaration of a C-ABI callee out of its call info.
	 */
	vm::code::FFIFunction ffiFunctionOf(const FunctionCallInfo& call_info) {
		vm::code::FFIFunction ffi_function;
		ffi_function.name
			= vm::code::Identifier(std::get<DVMFFIFunctionName>(call_info.call_target).name);
		ffi_function.signature.parameters
			= call_info.param_types | std::views::transform([](const auto& type) {
				  return vm::code::Identifier(vm::code::typeName(type));
			  })
		    | std::ranges::to<std::vector>();
		if_opt_some(call_info.return_type, result_type) {
			ffi_function.signature.result_types
				= { vm::code::Identifier(vm::code::typeName(result_type)) };
		}
		return ffi_function;
	}
}

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(const CallOperation& op) {
		CORE_ASSERT(
			op.call_info.param_types.size() == op.args.size(),
			"Argument count mismatch for function call: ",
			VISIT(op.call_info.call_target, callable, return callable.name)
		);

		auto call_result_storage = [&] -> base::Optional<DVMPlace> {
			if (op.call_info.return_type)
				return ctx->pushTempLocal(op.call_info.return_type.value(), "call_result");
			else
				return {};
		}();

		for (const auto& [arg_idx, func_arg, arg_type]:
		     std::views::zip(std::views::iota(0), op.args, op.call_info.param_types)) {
			CORE_DEV_LOG(Backend, "Initializing: ", typeName(arg_type), '\n');

			auto arg_name = base::strConcat("call", "_arg", arg_idx, "_");
			auto temp_arg = ctx->pushTempLocal(arg_type, arg_name, false);
			ctx->pushInstruction({ OpKind::mov, temp_arg.asArgument(), func_arg });
		}

		if (v_matches(op.call_info.call_target, DVMFFIFunctionName))
			ctx->program_context.insertFFIFunction(ffiFunctionOf(op.call_info));

		ctx->pushInstruction(
			{ OpKind::call, VISIT(op.call_info.call_target, callable, return callable.asArgument()) }
		);

		if (op.dest) {
			CORE_ASSERT(
				call_result_storage.has_value(), "Call with destination must have a return value"
			);
			ctx->maybeStoreResult(op.dest, { call_result_storage.value() });
		}
	}
}
