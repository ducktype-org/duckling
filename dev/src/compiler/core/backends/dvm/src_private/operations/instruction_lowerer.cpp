#pragma once

#include "instruction_lowerer.hpp"

#include "dvm_value.hpp"
#include "function_lowering_context.hpp"
#include "program_lowering_context.hpp"

#include "base/collections/optional.hpp"
#include "base/extend_cpp/variant_match.hpp"

namespace {}

namespace compiler::backend_vm::internal {

	// Normal instructions
	void InstructionLowerer::lower(const NoOpOperation&) {}

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

	void InstructionLowerer::lower(const MoveOperation& op) {
		// Otherwise, it's a simple assignment.
		ctx->storeResult(op.dest, op.src);
		return;
	}

	void InstructionLowerer::lower(const AddressOfOperation& op) {
		auto addr_temp = ctx->pushTempLocal(op.dest.getType(), "addr_of");

		if (op.src.isDirect()) {
			// If access to the variable is direct, we take it's address.
			ctx->pushInstruction({ OpKind::ref, addr_temp.asArgument(), op.src.asAnyArgument() });
		} else {
			// Otherwise, if the resolved source is accessed through a pointer
			// (AccessKind::Pointer), than we have the address in hand. We just move it.
			ctx->pushInstruction({ OpKind::mov, addr_temp.asArgument(), op.src.asArgument() });
		}
		ctx->storeResult(op.dest, { addr_temp, DVMPlace::AccessKind::Direct });
		return;
	}

	void InstructionLowerer::lower(const JumpOperation& op) {
		ctx->pushInstruction(
			instructions::Comment(
				base::StrID(
					base::strConcat("Terminator: ", base::enumToStr(lir_terminator.operation)).data()
				)
			)
		);

		ctx->pushInstruction({ OpKind::jmp, op.target });
	}

	void InstructionLowerer::lower(const BranchOperation& op) {
		ctx->pushInstruction(
			instructions::Comment(
				base::StrID(
					base::strConcat("Terminator: ", base::enumToStr(lir_terminator.operation)).data()
				)
			)
		);
		if (op.condition.is<DVMImmediate>()) {
			DVMImmediate cond = op.condition.get<DVMImmediate>();
			ctx->cleanUpRegisteredTemps();
			if (cond == DVMImmediate::boolean(true))
				ctx->pushInstruction({ OpKind::jmp, op.true_target });
			else
				ctx->pushInstruction({ OpKind::jmp, op.false_target });
		} else {
			ctx->pushInstruction({ OpKind::cmpEq, bool_arg, vm::opargs::Immediate{ 1 } });
			ctx->cleanUpRegisteredTemps();
			ctx->pushInstruction({ OpKind::jmpIf, true_block });
			ctx->pushInstruction({ OpKind::jmpIfNot, false_block });
		}
	}

	void InstructionLowerer::lower(const ReturnOperation& op) {
		ctx->pushInstruction(
			instructions::Comment(
				base::StrID(
					base::strConcat("Terminator: ", base::enumToStr(lir_terminator.operation)).data()
				)
			)
		);
		if_opt_some(op.value, ret_val) {
			// Since VM does not support `return X;` operation, we must move the value to
			// the ret_val local and then return.
			ctx->pushInstruction(
				{ OpKind::mov, getFunctionReturnValueLocal().asArgument(), op.value.value() }
			);
		}
		ctx->cleanUpRegisteredTemps();
		ctx->pushInstruction({ OpKind::ret });
	}
}
