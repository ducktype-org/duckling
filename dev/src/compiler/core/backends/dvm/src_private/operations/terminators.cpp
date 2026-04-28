#include "../dvm_value.hpp"
#include "../function_lowering_context.hpp"
#include "instruction_lowerer.hpp"

#include <base/collections/optional.hpp>

namespace compiler::backend_vm::internal {
	using namespace vm::code;

	void InstructionLowerer::lower(const JumpOperation& op) {
		ctx->pushInstruction({ OpKind::jmp, op.target.asArgument() });
	}

	void InstructionLowerer::lower(const BranchOperation& op) {
		if (op.condition.is<DVMImmediate>()) {
			auto cond = op.condition.get<DVMImmediate>();
			ctx->cleanUpRegisteredTemps();
			if (cond == DVMImmediate::boolean(true))
				ctx->pushInstruction({ OpKind::jmp, op.true_target.asArgument() });
			else
				ctx->pushInstruction({ OpKind::jmp, op.false_target.asArgument() });
		} else {
			ctx->pushInstruction({ OpKind::cmpEq, op.condition, vm::opargs::Immediate{ 1 } });
			ctx->cleanUpRegisteredTemps();
			ctx->pushInstruction({ OpKind::jmpIf, op.true_target.asArgument() });
			ctx->pushInstruction({ OpKind::jmpIfNot, op.false_target.asArgument() });
		}
	}

	void InstructionLowerer::lower(const ReturnOperation& op) {
		if_opt_some(op.value, ret_val) {
			// Since VM does not support `return X;` operation, we must move the value to
			// the ret_val local and then return.
			ctx->pushInstruction(
				{ OpKind::mov, ctx->getFunctionReturnValueLocal().asArgument(), ret_val }
			);
		}
		ctx->cleanUpRegisteredTemps();
		ctx->pushInstruction({ OpKind::ret });
	}
}
