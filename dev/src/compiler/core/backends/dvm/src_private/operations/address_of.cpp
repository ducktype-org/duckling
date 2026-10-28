// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../function_lowering_context.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

#include <program_lowering_context.hpp>

namespace compiler::backend_vm::internal {
	void InstructionLowerer::lower(const AddressOfOperation& op) {
		DVMPlace address = [&]() -> DVMPlace {
			if (op.src.isDirect()) {
				auto temp_type = ctx->program_context.pointerTypeExpr(op.src.getTypeExpr());
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

		const vm::code::TypeExpr dest_type
			= ctx->program_context.lowerAndKeepTslType(op.dest_layout.value());

		if (dest_type.isKind<vm::code::expr::CPointer>()) {
			if (address.getTypeExpr().isKind<vm::code::expr::Pointer>()) {
				const vm::code::TypeExpr src_cptr_type = ctx->program_context.pointerTypeExpr(
					ctx->program_context.keepTslType(op.src_layout),
					tsl::PointerTypeLayout::PointerKind::CPointer
				);
				DVMPlace cptr_temp = ctx->pushTempLocal(src_cptr_type, "addr_of_cptr");
				ctx->pushInstruction({ OpKind::cast, cptr_temp, address });
				address = cptr_temp;
			}

			if (address.getTypeExpr() != dest_type) {
				DVMPlace cast_temp = ctx->pushTempLocal(dest_type, "addr_of_cast");
				ctx->pushInstruction({ OpKind::cast, cast_temp, address });
				address = cast_temp;
			}
		}

		ctx->maybeStoreResult(op.dest, { address });
	}
}
