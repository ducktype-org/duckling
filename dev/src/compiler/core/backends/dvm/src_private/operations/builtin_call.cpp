#include "../function_lowering_context.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

#include <base/except/exceptions.hpp>

namespace compiler::backend_vm::internal {
	namespace {
		/**
		 * @brief The DVM char builtins operate on `manyptr char` values, which lower to a pointer
		 * to a dynamic table. `dynTableReAlloc` needs the pointed-to dynamic-table type, which is
		 * the pointer's inner type.
		 */
		base::StrID dynTableTypeName(const vm::code::TypeOfData& ptr_type) {
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
			// `dvm_char_alloc(size: u64) -> manyptr char`. The destination is a freshly created,
			// zero-initialized (i.e. null) dynamic-table pointer, so `dynTableReAlloc` allocates a
			// new table of `size` elements into it.
			CORE_ASSERT(op.args.size() == 1, "dvm_char_alloc expects 1 argument (size)");
			CORE_ASSERT(op.dest.has_value(), "dvm_char_alloc must have a destination");

			auto count      = ctx->forceToPlace(op.args.front(), "alloc_count");
			auto table_type = dynTableTypeName(op.dest->getType());
			ctx->pushInstruction({ OpKind::dynTableReAlloc,
			                       op.dest->asArgument(),
			                       vm::opargs::Type(table_type),
			                       count.asArgument() });
			break;
		}
		case lir::BuiltinFunctionKind::DvmCharRealloc: {
			// `dvm_char_realloc(p: manyptr char, size: u64)`. Reallocates the dynamic table under
			// `p` to `size` elements. `dynTableReAlloc` does not change the pointer value, so
			// operating on a copy of `p` is fine.
			CORE_ASSERT(op.args.size() == 2, "dvm_char_realloc expects 2 arguments (ptr, size)");

			auto table_ptr  = ctx->forceToPlace(op.args.at(0), "realloc_ptr");
			auto count      = ctx->forceToPlace(op.args.at(1), "realloc_count");
			auto table_type = dynTableTypeName(table_ptr.getType());
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
		}
	}
}
