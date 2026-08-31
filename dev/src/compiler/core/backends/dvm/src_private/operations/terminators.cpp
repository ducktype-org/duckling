#include "../dvm_value.hpp"
#include "../function_lowering_context.hpp"
#include "instruction_lowerer.hpp"

#include <base/collections/optional.hpp>

#include <vm/core/builtin_functions.hpp>

namespace compiler::backend_vm::internal {
	using namespace vm::code;

	void InstructionLowerer::lower(const JumpOperation& op) {
		ctx->pushDeinitsForInstr(op.scope_flags, this->pushed_deinits_for_instr);

		ctx->pushInstruction({ OpKind::jmp, op.target.asArgument() });
	}

	void InstructionLowerer::lower(const BranchOperation& op) {
		if (op.condition.is<DVMImmediate>()) {
			auto cond = op.condition.get<DVMImmediate>();

			ctx->cleanUpRegisteredTemps();
			// Branch instr has the deinits pushed between the condition evaluation and the jump.
			ctx->pushDeinitsForInstr(op.scope_flags, this->pushed_deinits_for_instr);

			if (cond == DVMImmediate::boolean(true))
				ctx->pushInstruction({ OpKind::jmp, op.true_target.asArgument() });
			else
				ctx->pushInstruction({ OpKind::jmp, op.false_target.asArgument() });
		} else {
			ctx->pushInstruction({ OpKind::cmpEq, op.condition, vm::opargs::Immediate{ 1 } });

			ctx->cleanUpRegisteredTemps();
			// Branch instr has the deinits pushed between the condition evaluation and the jump.
			ctx->pushDeinitsForInstr(op.scope_flags, this->pushed_deinits_for_instr);

			ctx->pushInstruction({ OpKind::jmpIf, op.true_target.asArgument() });
			ctx->pushInstruction({ OpKind::jmpIfNot, op.false_target.asArgument() });
		}
	}

	void InstructionLowerer::lower(const BranchIfNullOperation& op) {
		ctx->pushInstruction({ OpKind::cmpNull, op.pointer.asArgument() });

		ctx->cleanUpRegisteredTemps();
		// Deinits are pushed between the null check and the jumps, like in Branch.
		ctx->pushDeinitsForInstr(op.scope_flags, this->pushed_deinits_for_instr);

		ctx->pushInstruction({ OpKind::jmpIf, op.null_target.asArgument() });
		ctx->pushInstruction({ OpKind::jmpIfNot, op.not_null_target.asArgument() });
	}

	void InstructionLowerer::lower(const ReturnOperation& op) {
		ctx->pushDeinitsForInstr(op.scope_flags, this->pushed_deinits_for_instr);

		if (op.value.has_value() and op.value.value().is<DVMPlace>()
		    and op.value.value().get<DVMPlace>().getSpecialKind()
		            == DVMPlace::SpecialKind::ReturnValue) {
			// Since VM does not support `return X;` operation, we must move the value to
			// the ret_val local and then return.
			ctx->pushInstruction({ OpKind::ret });
			return;
		}

		if_opt_some(op.value, ret_val) {
			ctx->pushInstruction(
				{ OpKind::mov, ctx->getFunctionReturnValueLocal().asArgument(), ret_val }
			);
		}
		ctx->pushInstruction({ OpKind::ret });
	}

	void InstructionLowerer::lower(const UnreachableOperation& op) {
		const auto& abort_name
			= vm::builtins::getBuiltinFunctions()->at(vm::builtins::BuiltinFunctionID::Abort).name;

		lower(CallOperation{
			.call_info = { .call_target = DVMFunctionName{ .name = abort_name },
		                   .return_type = {},
		                   .param_types = {} },
			.args      = {},
			.dest      = {},
		});
		lower(ReturnOperation{
			.value       = {},
			.scope_flags = op.scope_flags,
		});
	}
}
