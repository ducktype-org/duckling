#include "../function_lowering_context.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

#include <base/except/exceptions.hpp>

namespace compiler::backend_vm::internal {
	namespace {
		/**
		 * @brief Given a pointer type as an argument, return the name of the type it points to.
		 * For a `manyptr T` that is the dynamic table of `T`, for a `ptr T` it is `T` itself.
		 */
		base::StrID extractPointeeTypeName(const vm::code::TypeOfData& ptr_type) {
			const auto* pointer = std::get_if<vm::code::PointerType>(&ptr_type);
			CORE_ASSERT(pointer != nullptr, "DVM builtin expects a `ptr`/`manyptr` operand");
			return pointer->inner;
		}
	}

	void InstructionLowerer::lower(const BuiltinCallOperation& op) {
		switch (op.kind) {
		case lir::BuiltinFunctionKind::DvmAllocArr: {
			CORE_ASSERT(op.args.size() == 1, "dvm_alloc_arr expects 1 argument (size)");
			CORE_ASSERT(op.dest.has_value(), "dvm_alloc_arr must have a destination");

			auto count      = ctx->forceToPlace(op.args.front(), "alloc_count");
			auto table_type = extractPointeeTypeName(op.dest->getType());
			ctx->pushInstruction({ OpKind::dynTableReAlloc,
			                       op.dest->asArgument(),
			                       vm::opargs::Type(table_type),
			                       count.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::DvmReallocArr: {
			CORE_ASSERT(op.args.size() == 2, "dvm_realloc_arr expects 2 arguments (ptr, size)");

			auto table_ptr  = ctx->forceToPlace(op.args.at(0), "realloc_ptr");
			auto count      = ctx->forceToPlace(op.args.at(1), "realloc_count");
			auto table_type = extractPointeeTypeName(table_ptr.getType());
			ctx->pushInstruction({ OpKind::dynTableReAlloc,
			                       table_ptr.asArgument(),
			                       vm::opargs::Type(table_type),
			                       count.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::DvmFreeArr: {
			// `dvm_free_arr(p: manyptr T)`. Frees the dynamic table under `p`.
			CORE_ASSERT(op.args.size() == 1, "dvm_free_arr expects 1 argument (ptr)");

			auto table_ptr = ctx->forceToPlace(op.args.front(), "free_arr_ptr");
			ctx->pushInstruction({ OpKind::free, table_ptr.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::DvmAlloc: {
			// `dvm_alloc() -> ptr T`. Allocates storage for a single `T`, the pointee type is read
			// off the destination `ptr T`.
			CORE_ASSERT(op.args.empty(), "dvm_alloc expects no arguments");
			CORE_ASSERT(op.dest.has_value(), "dvm_alloc must have a destination");

			auto pointee_type = extractPointeeTypeName(op.dest->getType());
			ctx->pushInstruction(
				{ OpKind::alloc, op.dest->asArgument(), vm::opargs::Type(pointee_type) }
			);
			break;
		}
		case lir::BuiltinFunctionKind::DvmFree: {
			// `dvm_free(p: ptr T)`. Frees the single object under `p`.
			CORE_ASSERT(op.args.size() == 1, "dvm_free expects 1 argument (ptr)");

			auto ptr = ctx->forceToPlace(op.args.front(), "free_ptr");
			ctx->pushInstruction({ OpKind::free, ptr.asArgument() });
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
		case lir::BuiltinFunctionKind::ListFree: {
			// `list_free(l: ref [T])`. Lists are not supported in DVM code generation yet.
			throw base::NotYetImplemented("Lists are not supported in DVM code generation yet.");
		}
		}
	}
}
