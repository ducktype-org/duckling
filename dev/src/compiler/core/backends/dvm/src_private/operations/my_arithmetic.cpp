#include "instruction_lowerer.hpp"
#include "dvm_operation.hpp"

namespace compiler::backend_vm::internal {

	// Normal instructions
	void InstructionLowerer::lower(const NoOpOperation& op) {}

	void InstructionLowerer::lower(const UnaryOperation& op) {
		// If instruction is of the form: a = OP b, then
		// we transform it to:
		// a = b;
		// a = OP a;

		if (op.dest.isDirect() && op.dest.is<DVMLocal>()) {
			// If output is a direct place we just use it.
			ctx->storeResult(op.dest, op.src);
			ctx->pushInstruction({ op.op, op.dest });
		} else {
			// Otherwise it's a global or indirect. We perform the operations on the
			// temporary and then store it in the indirect place.
			auto tmp = ctx->forceToLocal(op.src);
			ctx->pushInstruction({ op.op, tmp });
			ctx->storeResult(op.dest, { tmp, DVMPlace::AccessKind::Direct });
		}
		return;
	}

	void InstructionLowerer::lower(const BinaryOperation& op) {
		// In this case we assume we have a very general quadruple of the form:
		// output = arg1 OP arg2;

		// Force globals into locals. Immediates are allowed.
		DVMValue rhs = op.rhs.is<DVMGlobal>() ? DVMValue{ ctx->forceToLocal(op.rhs, "bin_rhs_tmp"),
			                                              DVMPlace::AccessKind::Direct }
		                                      : op.rhs;

		if (op.dest.isDirect() && op.dest.is<DVMLocal>()) {
			// If instruction is of the form: a = b OP c, then
			// we transform it to:
			// a = b;
			// a = a OP c;
			ctx->storeResult(op.dest, op.lhs);
			ctx->pushInstruction({ op.op, op.dest, rhs });
		} else {
			// Force globals into locals if needed.
			auto tmp = ctx->forceToLocal(op.lhs, "bin_tmp");
			ctx->pushInstruction({ op.op, tmp, rhs });
			ctx->storeResult(op.dest, { tmp, DVMPlace::AccessKind::Direct });
		}
		return;
	}

	void InstructionLowerer::lower(const CallOperation& op) {
		CORE_ASSERT(
			op.call_info.param_types.size() == func_args.size(),
			"Argument count mismatch for extern C function call: ",
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
			pushInstruction({ OpKind::mov, temp_arg.asArgument(), func_arg });
		}

		ctx->pushInstruction(
			{ OpKind::call, VISIT(op.call_info.call_target, callable, return callable.asArgument()) }
		);

		if (op.output) // TODOP: Store result should take in an optional
			storeResult(
				op.output.value(), { call_result_storage.value(), DVMPlace::AccessKind::Direct }
			);
	}

	void InstructionLowerer::lower(const AddressOfOperation& op) {
		auto addr_temp = pushTempLocal(
			ctx->program_context.lowerAndKeepTslType(lir_instruction.output->layout), "addr_of"
		);

		if (resolved_src.isDirect()) {
			// If access to the variable is direct, we take it's address.
			pushInstruction({ OpKind::ref, addr_temp.asArgument(), resolved_src.asAnyArgument() });
		} else {
			// Otherwise, if the resolved source is accessed through a pointer
			// (AccessKind::Pointer), than we have the address in hand. We just move it.
			pushInstruction({ OpKind::mov, addr_temp.asArgument(), resolved_src.asArgument() });
		}
		storeResult(output_dest, { addr_temp, DVMPlace::AccessKind::Direct });
		return;
	}

	void InstructionLowerer::lower(const CastOperation& op);
	void InstructionLowerer::lower(const MetaOperation& op);
	void InstructionLowerer::lower(const JumpOperation& op);
	void InstructionLowerer::lower(const BranchOperation& op);
	void InstructionLowerer::lower(const ReturnOperation& op);

}
