#include "../function_lowering_context.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

#include <base/except/exceptions.hpp>

namespace compiler::backend_vm::internal {
	namespace {
		/**
		 * @brief Given pointer to a dynamic table type as an argument,
		 * return the type of dynamic table on which the pointer point to.
		 */
		base::StrID extractPointeeTypeName(const vm::code::TypeOfData& ptr_type) {
			const auto* pointer = std::get_if<vm::code::PointerType>(&ptr_type);
			CORE_ASSERT(
				pointer != nullptr, "DVM char builtin expects a `manyptr` (pointer) operand"
			);
			return pointer->inner;
		}
	}

	void InstructionLowerer::lower(const BuiltinCallOperation& op) {
		switch (op.kind) {
		case lir::BuiltinFunctionKind::DvmCharAlloc: {
			CORE_ASSERT(op.args.size() == 1, "dvm_char_alloc expects 1 argument (size)");
			CORE_ASSERT(op.dest.has_value(), "dvm_char_alloc must have a destination");

			auto count      = ctx->forceToPlace(op.args.front(), "alloc_count");
			auto table_type = extractPointeeTypeName(op.dest->getType());
			ctx->pushInstruction({ OpKind::dynTableReAlloc,
			                       op.dest->asArgument(),
			                       vm::opargs::Type(table_type),
			                       count.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::DvmCharRealloc: {
			CORE_ASSERT(op.args.size() == 2, "dvm_char_realloc expects 2 arguments (ptr, size)");

			auto table_ptr  = ctx->forceToPlace(op.args.at(0), "realloc_ptr");
			auto count      = ctx->forceToPlace(op.args.at(1), "realloc_count");
			auto table_type = extractPointeeTypeName(table_ptr.getType());
			ctx->pushInstruction({ OpKind::dynTableReAlloc,
			                       table_ptr.asArgument(),
			                       vm::opargs::Type(table_type),
			                       count.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::DvmCharFree: {
			// `dvm_char_free(p: manyptr char)`. Frees the dynamic table under `p`.
			CORE_ASSERT(op.args.size() == 1, "dvm_char_free expects 1 argument (ptr)");

			auto table_ptr = ctx->forceToPlace(op.args.front(), "free_ptr");
			ctx->pushInstruction({ OpKind::free, table_ptr.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::BoxAlloc: {
			// `box_alloc(value: T) -> box T`. Allocate memory of size `T` and move the value into it.
			CORE_ASSERT(op.args.size() == 1, "box_alloc expects 1 argument (value)");
			CORE_ASSERT(op.dest.has_value(), "box_alloc must have a destination");

			auto& value = op.args.at(0);
			ctx->pushInstruction({ OpKind::alloc,
			                       op.dest->asArgument(),
			                       vm::opargs::Type(typeName(value.getType())) });
			auto value_place = ctx->forceToPlace(value, "box_value");
			ctx->pushInstruction(
				{ OpKind::store, op.dest->asArgument(), value_place.asAnyArgument() }
			);
			break;
		}
		case lir::BuiltinFunctionKind::BoxFree: {
			// `box_free(b: box T)`. Free the memory owned by the box.
			CORE_ASSERT(op.args.size() == 1, "box_free expects 1 argument (ptr)");

			auto box_ptr = ctx->forceToPlace(op.args.front(), "box_ptr");
			ctx->pushInstruction({ OpKind::free, box_ptr.asArgument() });
			break;
		}
		}
	}
}
