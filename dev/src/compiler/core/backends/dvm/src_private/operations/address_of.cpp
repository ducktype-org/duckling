#include "../function_lowering_context.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(const AddressOfOperation& op) {
		auto addr_temp = ctx->pushTempLocal(op.dest->getType(), "addr_of");

		if (op.src.isDirect()) {
			// If access to the variable is direct, we take it's address.
			ctx->pushInstruction({ OpKind::ref, addr_temp.asArgument(), op.src.asAnyArgument() });
		} else {
			// Otherwise, if the resolved source is accessed through a pointer
			// (AccessKind::Pointer), then we have the address in hand. We just move it.
			ctx->pushInstruction({ OpKind::mov, addr_temp.asArgument(), op.src.asArgument() });
		}
		ctx->maybeStoreResult(op.dest, { addr_temp, DVMPlace::AccessKind::Direct });
	}
}
