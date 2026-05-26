#include "../function_lowering_context.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(const BoxAllocOperation& op) {
		auto type = op.src.getType();
		std::cerr << op.dest.has_value() << "\n";
		ctx->pushInstruction(
			{ OpKind::alloc, op.dest->asArgument(), vm::opargs::Type(typeName(type)) }
		);
		auto src_place = ctx->forceToPlace(op.src);
		ctx->pushInstruction({ OpKind::store, op.dest->asArgument(), src_place.asAnyArgument() });
	}

	void InstructionLowerer::lower(const BoxFreeOperation& op) {
		auto src_place = ctx->forceToPlace(op.src);
		ctx->pushInstruction({ OpKind::free, src_place.asArgument() });
	}
}
