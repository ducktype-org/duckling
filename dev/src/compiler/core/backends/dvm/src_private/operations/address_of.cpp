#include "../function_lowering_context.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

#include <program_lowering_context.hpp>

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(const AddressOfOperation& op) {
		DVMPlace address = [&]() -> DVMPlace {
			if (op.src.isDirect()) {
				auto temp_type
					= ctx->program_context.getOrInsertPointerType(typeName(op.src.getType()));
				auto addr_temp = ctx->pushTempLocal(temp_type, "addr_of");
				// If access to the variable is direct, we take it's address.
				ctx->pushInstruction({ OpKind::ref, addr_temp.asArgument(), op.src.asAnyArgument() }
				);
				return addr_temp;
			}
			// Otherwise, if the resolved source is accessed through a pointer
			// (AccessKind::Pointer), then we have the address in hand. We just move it.
			return op.src;
		}();

		if (op.dest_layout.empty()) return;

		const vm::code::TypeOfData& dest_type
			= *ctx->program_context.lowerAndKeepTslType(op.dest_layout.value());

		if (v_matches(dest_type, vm::code::CPointerType)) {
			if (v_matches(address.getType(), vm::code::PointerType)) {
				const vm::code::TypeOfData& src_cptr_type
					= ctx->program_context.getOrInsertPointerType(
						ctx->program_context.keepTslType(op.src_layout),
						tsl::PointerTypeLayout::PointerKind::CPointer
					);
				DVMPlace cptr_temp = ctx->pushTempLocal(src_cptr_type, "addr_of_cptr");
				ctx->pushInstruction({ OpKind::cast, cptr_temp, address });
				address = cptr_temp;
			}

			if (vm::code::typeName(address.getType()) != vm::code::typeName(dest_type)) {
				DVMPlace cast_temp = ctx->pushTempLocal(dest_type, "addr_of_cast");
				ctx->pushInstruction({ OpKind::cast, cast_temp, address });
				address = cast_temp;
			}
		}

		ctx->maybeStoreResult(op.dest, { address });
	}
}
