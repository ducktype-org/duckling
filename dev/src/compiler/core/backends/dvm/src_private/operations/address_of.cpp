#include "../function_lowering_context.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"
#include "program_lowering_context.hpp"

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(const AddressOfOperation& op) {
		if (op.src.isDirect()) {
			auto temp_type = ctx->program_context.getOrInsertPointerType(op.src.getType());
			auto addr_temp = ctx->pushTempLocal(temp_type, "addr_of");
			// If access to the variable is direct, we take it's address.
			ctx->pushInstruction({ OpKind::ref, addr_temp.asArgument(), op.src.asAnyArgument() });
			ctx->maybeStoreResult(op.dest, { addr_temp });
		} else {
			// Otherwise, if the resolved source is accessed through a pointer
			// (AccessKind::Pointer), then we have the address in hand. We just move it.
			ctx->maybeStoreResult(op.dest, { op.src });
		}
	}
}
