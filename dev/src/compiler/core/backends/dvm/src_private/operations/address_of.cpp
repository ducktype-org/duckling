#include "instruction_lowerer.hpp"
#include "operations/dvm_operation.hpp"

namespace compiler::backend_vm::internal {
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
}
