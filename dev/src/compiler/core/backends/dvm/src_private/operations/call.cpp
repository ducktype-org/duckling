#include "../function_lowering_context.hpp"
#include "instruction_lowerer.hpp"
#include "operations/dvm_operation.hpp"

#include "base/except/exceptions.hpp"

#include <logger/logger.hpp>

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(const CallOperation& op) {
		CORE_ASSERT(
			op.call_info.param_types.size() == op.args.size(),
			"Argument count mismatch for function call: ",
			VISIT(op.call_info.call_target, callable, return callable.name)
		);

		auto call_result_storage = [&] -> base::Optional<DVMLocal> {
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

		ctx->pushInstruction(
			{ OpKind::call, VISIT(op.call_info.call_target, callable, return callable.asArgument()) }
		);

		if (op.dest) {
			CORE_ASSERT(
				call_result_storage.has_value(), "Call with destination must have a return value"
			);
			ctx->storeResult(op.dest, { call_result_storage.value(), DVMPlace::AccessKind::Direct });
		}
	}
}
